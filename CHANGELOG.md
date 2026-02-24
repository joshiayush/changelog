# Changelog

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

