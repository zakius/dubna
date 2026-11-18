// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

#include "extensions/renderer/api/firefox_menu_items_onclick.h"

#include <stdint.h>

#include <map>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/firefox_add_on.h"
#include "extensions/renderer/get_script_context.h"
#include "extensions/renderer/script_context.h"
#include "gin/converter.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function-callback.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-primitive.h"

namespace extensions {

namespace {

using Item = std::pair<ExtensionId, std::string>;

// The page that set each item's onclick last, as its ScriptContext's
// address: only compared, never followed. A page that closes leaves its
// items without an onclick, as in Firefox. Service workers' contexts run on
// other threads, hence the lock.
struct OnclickPages {
  base::Lock lock;
  std::map<Item, uintptr_t> pages;
};

OnclickPages& GetOnclickPages() {
  static base::NoDestructor<OnclickPages> onclick_pages;
  return *onclick_pages;
}

ScriptContext* GetCallingPage(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  return GetScriptContextFromV8ContextChecked(
      args.GetIsolate()->GetCurrentContext());
}

// The add-on of `page` if it is a Firefox add-on, else null.
const Extension* GetFirefoxAddOn(ScriptContext* page) {
  const Extension* extension = page->extension();
  if (!extension || !IsFirefoxAddOn(*extension)) {
    return nullptr;
  }
  return extension;
}

Item GetItem(const v8::FunctionCallbackInfo<v8::Value>& args,
             const Extension& extension) {
  CHECK(args[0]->IsString());
  return Item(extension.id(), gin::V8ToString(args.GetIsolate(), args[0]));
}

}  // namespace

void SetFirefoxOnclickPage(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(args.Length(), 2);
  CHECK(args[1]->IsBoolean());
  ScriptContext* page = GetCallingPage(args);
  const Extension* extension = GetFirefoxAddOn(page);
  if (!extension) {
    return;
  }
  Item item = GetItem(args, *extension);
  OnclickPages& onclick_pages = GetOnclickPages();
  base::AutoLock lock(onclick_pages.lock);
  if (args[1].As<v8::Boolean>()->Value()) {
    onclick_pages.pages[item] = reinterpret_cast<uintptr_t>(page);
  } else {
    onclick_pages.pages.erase(item);
  }
}

void IsFirefoxOnclickPage(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_EQ(args.Length(), 1);
  ScriptContext* page = GetCallingPage(args);
  const Extension* extension = GetFirefoxAddOn(page);
  if (!extension) {
    args.GetReturnValue().Set(true);
    return;
  }
  Item item = GetItem(args, *extension);
  OnclickPages& onclick_pages = GetOnclickPages();
  base::AutoLock lock(onclick_pages.lock);
  auto found = onclick_pages.pages.find(item);
  args.GetReturnValue().Set(found != onclick_pages.pages.end() &&
                            found->second == reinterpret_cast<uintptr_t>(page));
}

void ClearFirefoxOnclickPages(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  const Extension* extension = GetFirefoxAddOn(GetCallingPage(args));
  if (!extension) {
    return;
  }
  OnclickPages& onclick_pages = GetOnclickPages();
  base::AutoLock lock(onclick_pages.lock);
  std::erase_if(onclick_pages.pages, [extension](const auto& entry) {
    return entry.first.first == extension->id();
  });
}

}  // namespace extensions
