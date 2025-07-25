# Contributing guide

**Want to contribute? Great!**

All contributions are more than welcome ! This includes bug reports, bug fixes, enhancements, features, questions, ideas,
and documentation.

This document will hopefully help you contribute to pgexporter.

* [Legal](#legal)
* [Reporting an issue](#reporting-an-issue)
* [Setup your build environment](#setup-your-build-environment)
* [Building the main branch](#building-the-main-branch)
* [Before you contribute](#before-you-contribute)
* [Code reviews](#code-reviews)
* [Coding Guidelines](#coding-guidelines)
* [Discuss a Feature](#discuss-a-feature)
* [Development](#development)
* [Code Style](#code-style)

## Legal

All contributions to pgexporter are licensed under the [The 3-Clause BSD License](https://opensource.org/licenses/BSD-3-Clause).

## Reporting an issue

This project uses GitHub issues to manage the issues. Open an issue directly in GitHub.

If you believe you found a bug, and it's likely possible, please indicate a way to reproduce it, what you are seeing and what you would expect to see.
Don't forget to indicate your pgexporter version.

## Setup your build environment

For Red Hat RPM based distributions use the following command:

```
dnf install git gcc cmake make
```

in order to get the necessary dependencies.

## Building the main branch

To build the `main` branch:

```
git clone https://github.com/HighResMusicPlayer/hrmp.git
cd hrmp
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
cd src
./hrmp -?
```

## Before you contribute

To contribute, use GitHub Pull Requests, from your **own** fork.

Also, make sure you have set up your Git authorship correctly:

```
git config --global user.name "Your Full Name"
git config --global user.email your.email@example.com
```

We use this information to acknowledge your contributions in release announcements.

## Code reviews

GitHub pull requests can be reviewed by all such that input can be given to the author(s).

See [GitHub Pull Request Review Process](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/reviewing-changes-in-pull-requests/about-pull-request-reviews)
for more information.

## Coding Guidelines

* Discuss the feature
* Do development
  + Follow the code style
* Commits should be atomic and semantic. Therefore, squash your pull request before submission and keep it rebased until merged
  + If your feature has independent parts submit those as separate pull requests

## Discuss a Feature

You can discuss bug reports, enhancements and features in our [forum](https://github.com/HighResMusicPlayer/hrmp/discussions).

Once there is an agreement on the development plan you can open an issue that will used for reference in the pull request.

## Development

You can follow this workflow for your development.

Add your repository

```
git clone git@github.com:yourname/hrmp.git
cd hrmp
git remote add upstream https://github.com/HighResMusicPlayer/hrmp.git
```

Create a work branch

```
git checkout -b mywork main
```

During development

```
git commit -a -m "[#issue] My feature"
git push -f origin mywork
```

If you have more commits then squash them

```
git rebase -i HEAD~2
git push -f origin mywork
```

If the `main` branch changes then

```
git fetch upstream
git rebase -i upstream/main
git push -f origin mywork
```

as all pull requests should be squashed and rebased.

In your first pull request you need to add yourself to the `AUTHORS` file.

## Code Style

Please, follow the coding style of the project.

You can use the [uncrustify](http://uncrustify.sourceforge.net/) tool to help with the formatting, by running

```
./uncrustify.sh
```

and verify the changes.
