# How to Contribute

We would love to accept your patches and contributions to this project.

## Contribution process

### :raised_hand: Self Assigning to an Issue

If you find an issue you'd like to work on, simply type and submit a comment
with the phrase `.take` in it to get assigned by our github actions.

### :pencil2: Pull Request Guidelines

1. Code must finish continuous integration steps before it will be reviewed.
2. Commit messages should follow these guidelines here
   https://cbea.ms/git-commit/.
3. Pull requests should contain a single commit
4. Pull requests should be small and implement a single feature where possible.
   If it can be broken up into separate parts it most likely should be.
5. Each PR should have an associated issue with it. Exceptions are made for very
   small PRs such as fixing typos, fixing up documentation or removing
   unnecessary headers.

### :label: Releases

Merging a PR releases a new version when its title (the squashed commit's
subject) contains a keyword:

- `(minor)`: adds to the public API, e.g. a new interface, type or function.
- `(patch)`: fixes a bug, improves performance or code size, or changes
  anything under `experimental/`.

A PR without a keyword releases nothing. Each major version directory (`v4/`,
`v5/`) is released on its own, and only when the PR changes it. There is no
`(major)` keyword: a maintainer starts each major version by hand. See
[semantic_release.yml](https://github.com/libhal/ci#semantic_releaseyml) for
the details.

### Code Reviews

All submissions, including submissions by project members, require review. We
use [GitHub pull requests](https://docs.github.com/articles/about-pull-requests)
for this purpose.
