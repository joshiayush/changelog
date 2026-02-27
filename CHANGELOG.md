# Changelog

## changelog@v0.2.0 — 2026-02-27

### Feat

- feat: add author name in the commit entry by **Ayush Joshi** in [#98af9fb](https://github.com/joshiayush/changelog/commit/98af9fbb09d66bf80e187ccc6a925319876f1edc)

### Refactor

- refactor: highlight author name in bold with "@" suffix by **Ayush Joshi** in [#e89c526](https://github.com/joshiayush/changelog/commit/e89c5265485a99605393230df95b69ede4bf67b2)

### Revert

- revert: remove "@" from the Author Name in commit entry as it's not a clickable link by **Ayush Joshi** in [#b9c6b45](https://github.com/joshiayush/changelog/commit/b9c6b4599a23d5a8c14e4a00a67a885f5593f3bf)

### Fix

- fix: add "revert" to `CommitType` by **Ayush Joshi** in [#023404d](https://github.com/joshiayush/changelog/commit/023404decf79e5f50b61469665b77205a6a22951)
- fix: parse old changelogs written using changelog@v0.1.0 to filter from the new changelogs by **Ayush Joshi** in [#abcc5a9](https://github.com/joshiayush/changelog/commit/abcc5a93391c8f047c6b0b477db3363e3acd6c64)

### Docs

- docs: add changelog by **Ayush Joshi** in [#611f200](https://github.com/joshiayush/changelog/commit/611f200a81f0707145eb3cd70c6eb3da69e39685)


## changelog@v0.1.0 — 2026-02-24

### Feat

- feat: add automatic semantic versioning to changelog sections ([#04f6bca](https://github.com/joshiayush/changelog/commit/04f6bca95792ae21dff908ac831321b9c5f14bfc))
- feat: add changelog generator using libgit2 ([#88598b6](https://github.com/joshiayush/changelog/commit/88598b67792ed6694958b9a615fa77511455a6c7))
- feat: infer repository name from the repo url to format into the CHANGELOG.md file ([#e0fcd89](https://github.com/joshiayush/changelog/commit/e0fcd89a04c59e8ff70fde84cd738f65ce6499d5))
- feat: use `git_remote_lookup` to access repository remote url when not explicitly given ([#dd3664a](https://github.com/joshiayush/changelog/commit/dd3664abb32f4e7e4e2ac469e7736859ee1fd165))

### Refactor

- refactor: add include directory for all the header files ([#9500d34](https://github.com/joshiayush/changelog/commit/9500d346212682a40dff6214014442f671075c46))
- refactor: use macro definition for error validation ([#4149c7e](https://github.com/joshiayush/changelog/commit/4149c7eecd2a5bce77972c12dc3fb6c375b42c0d))

### Fix

- fix: make the first changelog entry to the seed version itself ([#e557742](https://github.com/joshiayush/changelog/commit/e557742efd573ac5455b95707537e3773d366c49))
- fix: stores logs under config_.repo_name (e.g. "charlie") instead of "" ([#5e4c143](https://github.com/joshiayush/changelog/commit/5e4c14397c665b67fd7ddeca0d1ea85e543eec08))

