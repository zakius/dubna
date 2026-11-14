# Nightly runs

The nightly update is a chain of T3 scheduled tasks in the dubna project. The router starts at 18:00, decides what is due, and starts one stage. Each stage does one job, writes one line to the log and starts the router again. The router then picks the next stage. So only one stage runs at a time, and never two builds at once.

| Task | Id | Job |
|---|---|---|
| dubna nightly · router | `scheduled-task:d1826b95-4bfd-4bd8-b749-46bfed2cef0f` | Status, space, what comes next, the final report, another round |
| dubna nightly · release | `scheduled-task:815cf2ed-72bb-4b97-8e17-63b6293f7159` | Release each main that is past its last release, and fix missing platforms |
| dubna nightly · update oganesson | `scheduled-task:8625b77b-636b-4947-950f-e635b35f3548` | Update the desktop browser |
| dubna nightly · update tennessine | `scheduled-task:d9b93584-ffe2-47ba-9feb-6e9f496ae974` | Update the Android browser |
| dubna nightly · tasks | `scheduled-task:59929e97-ca31-4513-828a-2bc54e01ce67` | A batch of open issues |
| dubna nightly · climb | `scheduled-task:cef297bb-8c2e-43b2-bcdb-fffdee0cefcc` | One step of the WebExtensions climb |
| dubna nightly · site | `scheduled-task:8f483e0f-4477-4461-b9c7-f69fdf612de5` | Bring the site up to date |

Every task is bound to one thread, the router's (`thread:project:950447f9-e52b-4764-aadc-a18c4c602b29:e68f137f-f5bb-4692-a8e6-db45394ca597`), and posts there. tools/usage_pace.py counts the usage of this thread apart, so a new thread needs its id there too. Each run starts a fresh session, so the log, not the thread, carries the state. A run started while the thread is busy waits in the thread's queue, so runs never overlap.

The stages have webhook triggers, so they never start on their own. `run_scheduled_task_now` refuses webhook tasks, so the router starts a stage by posting to the task's `webhookUrl` (from `list_scheduled_tasks`; the URL is a secret, so never write it to the log or Git). A stage starts the router with `run_scheduled_task_now`.

Don't open threads: for a helper agent, use the native Agent tool (`run_in_background: false`), never `delegate_task` or `t3_thread_launch`.

## The log

The chain keeps its state in `/home/zakius/dev/dubna/work/nightly.log` (not in Git). Each line is a UTC time, then an event:

```
2026-11-11T17:00:12Z begin
2026-11-11T17:02:40Z finding update oganesson helium 0.9.3.1 (rule 1)
2026-11-11T17:02:41Z finding status: current tennessine
2026-11-11T17:02:41Z space main 71% oracle 66%
2026-11-11T17:02:50Z start update-oganesson update helium 0.9.3.1
2026-11-11T19:41:03Z end update-oganesson landed v0.9.3.1 released at release-20261111-193002, Linux and Windows
2026-11-11T19:41:30Z start site
...
2026-11-11T21:10:00Z end chain landed
```

- `begin` opens a chain. Only the router writes `begin`, `round`, `finding`, `space`, `start` and `end chain` lines.
- `round <N>` comes right after the `begin` of a chain that the router started as another round (see "Another round"). A chain begun at 18:00 or by hand has no `round` line.
- `start <stage> [details]` comes just before the router starts the stage. The details are what the stage works on.
- `end <stage> <result> <summary>` is the stage's last line. The result is `landed` (pushed something), `nothing`, `skipped` or `stopped` (stopped before a push, or left something broken). The router writes `end <stage> died` for a stage whose thread stopped without an end line.

"Tonight" and "this chain" mean the lines after the last `begin`. "06:00" means the first 06:00 after that `begin`, so a chain begun by hand in the morning works on until the next 06:00. That deadline is only for a stage's own work. It never lets the router start another round: "Another round" decides that.

## Another round

When a chain ends `landed`, the router may begin another chain at once. Chains may run at any hour, but the router starts another round only when all of these hold:

1. The chain ended `end chain landed`.
2. The local time now (`TZ=Europe/Warsaw date +%H:%M`) is not from 05:00 to 10:00. Judge by the clock now, never by a `begin` line.
3. Fewer than 5 rounds in a row: the chain that just ended has no `round` line, or its `round` number is under 5.
4. tools/usage_pace.py prints "headroom: yes" first ("headroom: unknown" counts as no). It projects only the work outside this thread, so the rounds don't count against themselves.
5. No other T3 thread is active.

For another round, the router appends `begin`, then `round <N>`, where N is the ended chain's round number plus one (1 if it had none). So a chain begun at 18:00 or by hand is followed by at most 5 more.

## Starting a stage

1. git -C /home/zakius/dev/dubna pull -q, read this file, and read the stage's own prompt.
2. Read the log from the last `begin`. Its last line should be `start <this stage>`: its details are your job, and the end lines before it say what landed tonight.
3. If the last line is not your `start` line, someone started the stage by hand. Append `start <stage> manual` yourself, do the job, and write the end line, but don't start the router at the end.

## Ending a stage

1. Post the report in your thread. It is for the maintainer, who reviews it later: every fix, judgement call and flaky test, and anything failed or skipped.
2. Append the end line: `echo "$(date -u +%FT%TZ) end <stage> <result> <one-line summary>" >> /home/zakius/dev/dubna/work/nightly.log`.
3. Unless the stage was started by hand, start the router as the last tool call: load `mcp__t3-code__run_scheduled_task_now` (ToolSearch `select:mcp__t3-code__run_scheduled_task_now`) and call it with the router's id from the table. Do nothing after it.

Every update goes out tonight, with no green light needed: try hard. Fix failing patches, carry new upstream patches, and work around build breaks. If it builds, the tests pass and the Fable review is done, push and release it, then report every fix and judgement call. When a stage is clean, take it to the end without stopping to report in between.

The maintainer may post a question in the thread while a stage runs. Answer it in a few lines, then carry on with the stage in the same turn. A question is not a request to stop, and never end the turn to wait for a reply. Don't ask permission for a step these rules already allow, such as a push or a release tag after the Fable review.

## Fable review before the push

Every push to a main (oganesson, tennessine or dubna: an update, a batch, a climb step, a fix, a submodule move, a site or tools change) waits for a review by Fable. A push to a task-* or climb-* branch doesn't.

1. When the commits are final (built and tested), make one Agent call: model `fable`, subagent_type `general-purpose`, run_in_background false. Give it the worktree, the range (origin/main..HEAD), what the change is for (issues, upstream version, policy rule) and the repo's docs to read. Give it the code, not your conclusions. Ask for real defects only, most severe first, each with file:line and how it fails: wrong behaviour, a missed upstream change, a test that doesn't test what it says, a security hole.
2. Fix each finding you agree with in its own commit, and build and test again. Say in the report why you reject the others. After fixes, one more review of the new commits only. Don't start a third: report what is left.
3. If the review can't run (the Fable limit is reached, or the call fails twice), don't push. Keep the branch, end the stage `stopped` with "no Fable review", and give the branch and worktree. One exception: a `zeroday` update pushes and releases anyway, and its report says the review was skipped.

The report lists each finding, its fix or why it was rejected.

## Releasing

The dubna README's "Releases" section explains this. A merge or push to main only merges: it builds and publishes nothing. A release is a release-* tag pushed on main:

```sh
git -C <checkout> fetch -q origin main && tag="release-$(date -u +%Y%m%d-%H%M%S)" && git -C <checkout> tag "$tag" origin/main && git -C <checkout> push origin "$tag"
```

In oganesson and tennessine the tag starts release.yml, which releases main's tip. In dubna it starts site.yml, which publishes the site.

Nothing ships without the tag, so never stop at the push. Every push to a browser's main that changes the shipped browser (an update, a tracker batch, a climb step, a dubna submodule move) is followed by a release-* tag in the same stage. So is every push to dubna's main that changes site/. Tool-only, docs-only and tests-only pushes (dubna tools, oganesson tools/e2e) need no tag.

After the tag, find its run (gh run list -R zakius/<repo> --workflow release.yml -L 3, the run for your tag) and wait for it with a time limit (gh run watch <id> -R zakius/<repo> --exit-status). If no run starts within a few minutes, check that the tag reached origin (git ls-remote --tags origin "$tag") and start the run by hand (gh workflow run release.yml -R zakius/<repo>). If the run fails, read gh run view <id> --log-failed. If the cause is clear and small, fix it, push and tag again, else report. When the release workflow fails after the push, report it; don't revert the push. Report the tag, the run and its result, and the release's URL.

A repo is released when:

- oganesson or tennessine: main's tip is the commit of the newest `v` release tag (git -C <checkout> describe --tags --match 'v*' origin/main, or gh release list -R zakius/<repo> -L 1).
- dubna's site: no commit since the newest release-* tag touches site/.

## Every platform ships

Each release must carry Linux and Windows for oganesson, and the APK for tennessine.

oganesson's Windows step is continue-on-error, so a green run can still post a release that says "The Windows build failed; this release has the Linux packages only." After every oganesson release run, check the Windows step's log (gh run view <id> -R zakius/oganesson --log: the build runs in the background, and the "Windows packages, to the GitHub release and the site" step prints its log) and the release's assets (gh release view <tag> -R zakius/oganesson --json assets). The only accepted reason for a missing Windows build is that helium-windows doesn't have the tag yet (say so). Any other Windows failure is a build failure to fix in the same stage, usually in windows/container-build.sh or windows/late. The cross build's Linux-hosted tools sometimes need DEPS that only Linux checkouts fetch: see fetch_linux_dep. Fix it, push, tag a release, and wait for a run that publishes Windows.

For tennessine, check that its release has the APK and that the F-Droid repository has it.

A release is complete only when it has every platform.

## Flaky tests

This applies everywhere: updates, tracker tasks, climb steps and release workflow runs. A test that fails once and passes on a rerun is a bug to fix in the same stage, never something to rerun past.

1. Measure it with --repeat 20 (oganesson: ./oganesson.py test --release --repeat 20 <suite.test>, under the builds lock).
2. Run the same against the installed previous release (oganesson: --binary /opt/oganesson/oganesson) to tell a regression from an old flake.
3. Find the cause: instrument the test, and read the kept profile and the browser.log that the harness names.
4. When the product is at fault (a race that also hits users, such as lost writes at shutdown), fix the product. When only the test is at fault, fix the test (wait for the final state instead of asserting a snapshot). It is fixed when 20 runs in a row pass.

Meanwhile, rerunning the failed release job (gh run rerun <id> --failed) to get the release out is fine, but never instead of the fix. Make one commit per flake fix. Commits that touch only tools/e2e need no release: push them with the next release, or on their own without a tag. List each flake, its cause and its fix in the report.

## Worktrees and caches

Keep what speeds up later work. Never delete worktrees, their submodule checkouts, the shared work/ trees, build or download caches, docker images, or prepared patch-check trees (work/rebase, work/chromium/src). Delete only true garbage (merged branches, scratch logs). When disk use is above 85%, delete the least valuable thing first, and say so.

Each repo has one standing worktree that every run reuses: /home/zakius/dev/<repo>-update (not /tmp: a Chromium build doesn't fit there), and /home/zakius/dev/dubna-site for the site. If it doesn't exist yet, create it once (git worktree add), link the main checkout's build scratch into it (ln -s /home/zakius/dev/<repo>/work work) so that builds reuse its tree, caches and package history and share its build lock, and keep it afterwards.

Each run: git fetch, check out a new branch from origin/main (git checkout -B), then git submodule update --init (it only moves what changed). If the worktree has uncommitted or unpushed work from an earlier run, don't discard it. Report it and stop for the maintainer, or carry it on if it is the same job. Don't commit in the main checkout.

When the release workflow succeeded with every platform, delete only the merged branch (git branch -d). When a stage stopped before the push, keep the branch, and give its name and the worktree's path in the report.

## Long jobs

Run long jobs detached, with the EXIT line written inside the detached shell (an echo after setsid runs at once, because setsid forks):

```sh
setsid nohup sh -c '<commands>; echo "EXIT $?"' > LOG 2>&1 < /dev/null &
```

Logs go in the main checkout's work/. Wait for the EXIT line, or on the job's PID, with a time limit (12 hours per build).

Before calling a build or test failure real, read uptime. At a load above 8, it counts only if a rerun at low load also fails.

Builds may run at any hour, and a chain carries on to the end whenever it started, even when started by hand during the day. From 08:00 to 16:00 the system gives builds fewer CPUs, so they are slower; that is no reason to pause or wait. The CPU limits come from the system (docker.slice), not the scripts: don't add any.

## Doing an update

The update stages follow this, together with their own prompts.

- Rule 3 (zero-day) takes the upstream's first release with the fix, even into a new milestone. If the upstream has no such release, bump the browser's Chromium version ourselves and fix the upstream patches that fail. Those fixes stay ours until the upstream catches up.
- Rule 2 (milestone): dubna/milestone.txt moves with both browsers. Commit and push it in dubna, and move the dubna submodule in each browser.

What needs fixing:

- A patch that doesn't apply with zero fuzz. When a shared dubna patch fails, report that first: it is the case the rule 1 experiment watches for. Never edit an upstream's own patches, only ours, or drop theirs if nothing builds on it.
- A drop that no longer matches its patch, or now catches a patch it wasn't meant for: fix the drop line.
- A new upstream patch that adds or removes a user-visible feature, changes a default, or touches something we drop or override: carry it as the upstream ships it, and list it for review.
- A build or test failure: fix it as a separate commit if the cause is clear and the fix is small. Otherwise stop, don't push, and leave it for the maintainer. Fix a flaky test as "Flaky tests" says.

Commit messages follow the repo's own: the version, the policy rule that let it in, whether every patch applies, the fixes made, and anything new upstream worth a line. The push is a fast-forward of main: if main moved, rebase onto it. Push the commit you built and tested as it is: the release then finds the tree prepared and the build done (oganesson compares the inputs, tennessine also takes its versionCode from the commit's time), and compiles nothing again.
