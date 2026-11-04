// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "chrome/browser/app_links/app_links.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/app_links/app_links_platform.h"
#include "chrome/browser/app_links/app_scope_rules.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/new_window_form_post.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/app_links/app_link_host.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace app_links {

namespace {

// Marks a popup (window.open with window features) that an app's window
// opened and that stayed: sign-in flows open the provider in one, and its
// links and its opener stay with it. On desktop a popup has its own window
// type; on Android it is one more tab of the app's activity.
class AppPopup : public content::WebContentsUserData<AppPopup> {
 private:
  explicit AppPopup(content::WebContents* contents)
      : content::WebContentsUserData<AppPopup>(*contents) {}
  friend class content::WebContentsUserData<AppPopup>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(AppPopup);

// The app whose own window shows `contents`.
std::optional<std::string> AppId(content::WebContents* contents) {
  if (!contents || AppPopup::FromWebContents(contents)) {
    return std::nullopt;
  }
  return PlatformAppId(contents);
}

PrefService& Prefs(content::WebContents* contents) {
  return *Profile::FromBrowserContext(contents->GetBrowserContext())->GetPrefs();
}

bool InScope(content::WebContents* contents,
             const std::string& app_id,
             const GURL& url) {
  return UserAppScopeOverride(Prefs(contents), app_id, url)
      .value_or(PlatformIsUrlInScope(contents, url));
}

// Whether the user made the scope of `contents`' app strict: every page out
// of it leaves by any route, as an excluded page does.
bool HasStrictScope(content::WebContents* contents) {
  std::optional<std::string> app_id = AppId(contents);
  return app_id && IsUserAppScopeStrict(Prefs(contents), *app_id);
}

// Whether a page of `contents` reaching `url` leaves the window's web app: by
// one of the user's exclusions, which no route gets past, or with
// `out_of_scope` or a strict scope, by leaving the app's scope.
bool LeavesApp(content::WebContents* contents,
               const GURL& url,
               bool out_of_scope) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  std::optional<std::string> app_id = AppId(contents);
  if (!app_id) {
    return false;
  }
  PrefService& prefs = Prefs(contents);
  return IsUrlExcludedFromUserAppScope(prefs, *app_id, url) ||
         ((out_of_scope || IsUserAppScopeStrict(prefs, *app_id)) &&
          !InScope(contents, *app_id, url));
}

// Whether `contents` may open a URL that leaves the app now; see
// ExclusionObserver::MayOpen.
bool MayOpen(content::WebContents* contents);

class OutOfScopeLinkThrottle : public content::NavigationThrottle {
 public:
  // `link`: the page's own link click, which also leaves the app for being
  // out of its scope; any other navigation only for an exclusion.
  OutOfScopeLinkThrottle(content::NavigationThrottleRegistry& registry,
                         bool link)
      : content::NavigationThrottle(registry), link_(link) {}

  ThrottleCheckResult WillStartRequest() override { return Check(link_); }

  // Every hop, the last one included; a link redirected in scope stays.
  ThrottleCheckResult WillRedirectRequest() override { return Check(false); }

  const char* GetNameForLogging() override { return "OutOfScopeLinkThrottle"; }

 private:
  ThrottleCheckResult Check(bool link) {
    content::NavigationHandle* handle = navigation_handle();
    content::WebContents* contents = handle->GetWebContents();
    // A post's body can't go along; a 302 or 303 after it is a GET again.
    if (handle->IsPost() || !LeavesApp(contents, handle->GetURL(), link)) {
      return PROCEED;
    }
    // A script that keeps trying, or keeps landing on a redirect out, opens
    // one tab per user input, not one per try.
    if (MayOpen(contents)) {
      OpenInDefaultBrowser(contents, handle->GetURL(),
                           /*background=*/!handle->HasUserGesture());
    }
    return CANCEL_AND_IGNORE;
  }

  const bool link_;
};

// The page's frame asks AppLinkHost before it changes its address, so such
// a change doesn't happen; this is for one the frame didn't ask about, such
// as from a page that moved into the app window while it changed it.
// Same-document navigations (pushState, replaceState, fragments) commit
// without throttles: one that reaches an excluded URL, or from a click one
// out of the app's scope, as a link there would (routers such as Facebook's
// turn link clicks into pushState), opens it outside the app after the fact
// and takes the window back. A pushed entry goes back, as the Back button
// would, to the last entry in the app, so the page's router shows it again;
// for a replaced entry, the page before loads again in its place. The window
// can show the excluded page's content until then. One URL goes out per
// user input: a router that answers the take-back by navigating out again,
// to the same URL or the one it resolves to (Facebook's /share/ links), is
// taken back again without opening another tab in the default browser, a few
// times, so a page that never gives up can't keep the window busy. The
// observer also has an app window update where it shows that its page is out
// of the app when the user's scope rules change.
class ExclusionObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ExclusionObserver> {
 public:
  void DidGetUserInteraction(const blink::WebInputEvent& event) override {
    take_backs_since_input_ = 0;
    opened_since_input_ = false;
  }

  // Whether a URL that leaves the app may open in the default browser: one
  // per user input, so a page that tries again, or a router that answers
  // with another URL, can't open a tab per try.
  bool MayOpen() { return !std::exchange(opened_since_input_, true); }

  void DidFinishNavigation(content::NavigationHandle* handle) override {
    // Not history navigations: a Back or Forward that loops between two
    // excluded entries would never end.
    if (!handle->IsInPrimaryMainFrame() || !handle->IsSameDocument() ||
        !handle->HasCommitted() || !handle->IsRendererInitiated() ||
        handle->IsHistory()) {
      return;
    }
    if (!LeavesApp(web_contents(), handle->GetURL(),
                   /*out_of_scope=*/handle->HasUserGesture())) {
      return;
    }
    if (take_backs_since_input_ == kMaxTakeBacksPerInput) {
      return;
    }
    ++take_backs_since_input_;
    if (MayOpen()) {
      OpenInDefaultBrowser(web_contents(), handle->GetURL(),
                           /*background=*/!handle->HasUserGesture());
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&TakeBack, web_contents()->GetWeakPtr(),
                       handle->GetURL(),
                       handle->DidReplaceEntry()
                           ? handle->GetPreviousPrimaryMainFrameURL()
                           : GURL()));
  }

 private:
  explicit ExclusionObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents),
        content::WebContentsUserData<ExclusionObserver>(*contents) {
    scope_rules_registrar_.Init(&Prefs(contents));
    scope_rules_registrar_.Add(
        kUserAppScopeRules,
        base::BindRepeating(&ExclusionObserver::OnScopeRulesChanged,
                            base::Unretained(this)));
  }
  friend class content::WebContentsUserData<ExclusionObserver>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  void OnScopeRulesChanged() {
    if (AppId(web_contents())) {
      app_links::OnScopeRulesChanged(web_contents());
    }
  }

  // Back from `excluded`: with a `replaced` URL, to that URL in its place;
  // otherwise, unless the page is back in the app by now, to the last entry
  // in the app, past every entry the page pushed out of it meanwhile
  // (Facebook pushes a /share/ link and then the URL it resolves to). Both
  // are browser navigations, which the exclusions leave alone.
  static void TakeBack(base::WeakPtr<content::WebContents> contents,
                       const GURL& excluded,
                       const GURL& replaced) {
    if (!contents) {
      return;
    }
    content::NavigationController& controller = contents->GetController();
    if (replaced.is_valid()) {
      if (contents->GetLastCommittedURL() == excluded) {
        content::NavigationController::LoadURLParams params(replaced);
        params.should_replace_current_entry = true;
        params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
        controller.LoadURLWithParams(params);
      }
      return;
    }
    if (!LeavesApp(contents.get(), contents->GetLastCommittedURL(),
                   /*out_of_scope=*/true)) {
      return;
    }
    for (int i = controller.GetLastCommittedEntryIndex() - 1; i >= 0; --i) {
      if (!LeavesApp(contents.get(), controller.GetEntryAtIndex(i)->GetURL(),
                     /*out_of_scope=*/true)) {
        controller.GoToIndex(i);
        return;
      }
    }
  }

  PrefChangeRegistrar scope_rules_registrar_;
  static constexpr int kMaxTakeBacksPerInput = 3;
  // Navigations taken back since the user's last input to the page.
  int take_backs_since_input_ = 0;
  bool opened_since_input_ = false;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(ExclusionObserver);

class AppLinkHost : public blink::mojom::AppLinkHost {
 public:
  explicit AppLinkHost(content::GlobalRenderFrameHostId frame)
      : frame_(frame) {}

  void OpenOutsideApp(const GURL& url,
                      bool link,
                      bool background,
                      OpenOutsideAppCallback callback) override {
    content::RenderFrameHost* frame =
        content::RenderFrameHost::FromID(frame_);
    content::WebContents* contents =
        frame && frame->IsInPrimaryMainFrame()
            ? content::WebContents::FromRenderFrameHost(frame)
            : nullptr;
    if (!contents || !LeavesApp(contents, url, link)) {
      std::move(callback).Run(false);
      return;
    }
    ExclusionObserver::CreateForWebContents(contents);
    if (ExclusionObserver::FromWebContents(contents)->MayOpen()) {
      OpenInDefaultBrowser(contents, url, background);
    }
    std::move(callback).Run(true);
  }

 private:
  const content::GlobalRenderFrameHostId frame_;
};

bool MayOpen(content::WebContents* contents) {
  ExclusionObserver* observer = ExclusionObserver::FromWebContents(contents);
  return !observer || observer->MayOpen();
}

}  // namespace

bool IsInAppWindow(content::WebContents* contents) {
  return AppId(contents).has_value();
}

bool MaybeOpenNewContentsOutsideApp(content::WebContents* source,
                                    const GURL& url,
                                    WindowOpenDisposition disposition,
                                    bool post,
                                    content::WebContents* new_contents) {
  if (!IsInAppWindow(source)) {
    return false;
  }
  const bool popup = disposition == WindowOpenDisposition::NEW_POPUP;
  // Popups stay, unless the scope is strict: sign-in flows open the provider
  // in one and need its opener. So do form posts, which only Chromium can
  // submit.
  if ((disposition != WindowOpenDisposition::NEW_FOREGROUND_TAB &&
       disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB &&
       disposition != WindowOpenDisposition::NEW_WINDOW &&
       (!popup || !HasStrictScope(source))) ||
      post || (new_contents && content::IsNewWindowForFormPost(new_contents)) ||
      !LeavesApp(source, url, /*out_of_scope=*/true)) {
    if (popup && new_contents) {
      AppPopup::CreateForWebContents(new_contents);
    }
    return false;
  }
  // A middle click or Ctrl+click leaves the default browser behind the app
  // window, as it leaves a tab in the background.
  OpenInDefaultBrowser(
      source, url, disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB);
  return true;
}

void BindAppLinkHost(
    content::RenderFrameHost* frame,
    mojo::PendingReceiver<blink::mojom::AppLinkHost> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<AppLinkHost>(frame->GetGlobalId()), std::move(receiver));
}

void MaybeAddThrottle(content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.IsInPrimaryMainFrame()) {
    return;
  }
  // On every page: a tab can move into an app window with its page loaded.
  ExclusionObserver::CreateForWebContents(handle.GetWebContents());
  // Only the page's navigations: not history or reloads, nor the app's
  // launch.
  if (!handle.IsRendererInitiated() ||
      handle.GetReloadType() != content::ReloadType::NONE ||
      (handle.GetPageTransition() & ui::PAGE_TRANSITION_FORWARD_BACK)) {
    return;
  }
  // Out of scope, only the page's own link clicks: not redirects or form
  // posts, and not scripts navigating, even from a click handler (sign-in
  // buttons that fetch the provider's URL and assign it). Exclusions, and
  // with a strict scope every URL out of it, all.
  registry.AddThrottle(std::make_unique<OutOfScopeLinkThrottle>(
      registry, handle.WasInitiatedByLinkClick() && handle.HasUserGesture() &&
                    !handle.IsPost()));
}

}  // namespace app_links
