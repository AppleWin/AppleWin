# Contributing to AppleWin

First of all, thanks for taking the time to contribute!

## How can I contribute?

### Reporting bugs

Before submitting a bug, search the existing issues to see if this bug has already been raised. If it has, then add a comment to this existing issue with your extra details.

If raising a new bug issue, then include the following details:
- AppleWin version
- OS version (eg. Windows 7-64, Windows 10 1909)
- A complete set of steps to reproduce the issue

Optionally:
- AppleWin log file (created using `applewin.exe -log`)
- Save-state file (.aws.yaml file) and any associated floppy or harddisk image files
- Screenshots

### Suggesting enhancements

Before submitting an enhancement, search the existing issues to see if this enhancement has already been raised. If it has, then add a comment to this existing issue with your extra details.

### Pull Requests

The process described here has several goals:
- Maintain AppleWin's quality
- Enforce a workable solution for AppleWin maintainers to review contributions

Please review the simple [Coding Conventions](https://github.com/AppleWin/AppleWin/blob/master/docs/CodingConventions.txt).

Smaller PRs are highly desirable, as they should be simpler to review and approve. Large changes are likely to be rejected or not get looked at (resulting in them going stale, and ultimately diverging further from the mainline repo).

For large changes being submitted for review, then it's HIGHLY recommended to split the large PR into smaller PRs, and submit them piece by piece. This means that no dependencies can exist between each smaller PR.

For a PR, don't make changes that are unrelated to the PR. These can be done in separate PRs.

Follow the coding style in the source file(s) that are being changed. Since this is a mature codebase, then bear in mind that different coding styles can exist in different source files.

For new (large) features, then link the PR back to an enhancement issue, where the proposed feature had been discussed with AppleWin maintainers first and justified using suitable use-cases. In this enhancement issue be sure to include a specification of the feature, and a design if necessary. Having a design (doc, diagrams) that explains the logic/algorithms/protocol etc will help in the long term maintenance of this feature.

When changing project metadata files (eg. .sln, .vcproj, .rc) then different versions of Visual Studio may decide to reorder or re-format other sections of the file. This can result in lots of churn in the file each time a trivial edit is made. So before committing the PR, check the difference, and if there's been lots of unnecessary changes then just make the required change using a non-Visual Studio editor.
