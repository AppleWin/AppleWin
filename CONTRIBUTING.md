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

Thank you for wanting to make the project better! However, before you submit a PR (Pull Request) please read the following carefully as the process described here has several goals:

- Maintain AppleWin's quality,
- Enforce a workable solution for AppleWin maintainers to review contributions, and
- Explain the development philosophy.

Firstly,

# Do NOT submit one big patch!

The AppleWin developers work on AppleWin in their spare time. This means we have limited time to review PRs. Smaller PRs are highly desirable, as they should be simpler to review and approve. Large changes are likely to be rejected or not get looked at (resulting in them going stale, and ultimately diverging further from the mainline repo).

For large changes being submitted for review, then it's HIGHLY recommended to split the large PR into smaller PRs, and submit them piece by piece. This means that no dependencies can exist between each smaller PR.

For a PR, don't make changes that are unrelated to the PR as this adds unnecessary noise and time to review. These can and should be done in separate PRs.

Next, please make sure your code matches the existing style by reviewing the simple [Coding Conventions](https://github.com/AppleWin/AppleWin/blob/master/docs/CodingConventions.txt).

Follow the coding style in the source file(s) that are being changed. Since this is a mature codebase, then bear in mind that different coding styles can and do exist in different source files due to many different authors over the past 25+ years.

For new (large) features, then link the PR back to an enhancement issue, where the proposed feature had been discussed with AppleWin maintainers first and justified using suitable use-cases. In this enhancement issue be sure to include a specification of the feature, and a design if necessary. Having a design (doc, diagrams) that explains the logic/algorithms/protocol etc will help in the long term maintenance of this feature.

When changing project metadata files (eg. .sln, .vcproj, .rc) then different versions of Visual Studio may decide to reorder or re-format other sections of the file. This can result in lots of churn in the file each time a trivial edit is made. So before committing the PR, check the difference, and if there's been lots of unnecessary changes then just make the required change using a non-Visual Studio editor.

When submitting UI changes please discuss WHY you are making changes.

* **Our userbase expects things to work the way they are used to.**  Changing default behavior without a "migration path" causes users to submit bug reports asking us to revert changes which wastes everyone's time.

* **Functionality should be prioritized over Form.** The _entire_ point of UI is to empower the user to do the thing they want to do and _then get out of their way._  A pretty UI, but one that frustrates users, is not empowering them.

* Also keep in mind many so called modern UI/UX "experts" tend to make changes for the sake of change in order to sell a product. Worse they tend to focus on mobile design even when it makes little or no sense on a desktop platform. There is a time and a place to focus on Form but as a mature project we are more interested in stability and functionality than throwing on a fresh coat of paint that more likely introduces a new set of bugs.

Understand that not everyone will agree to UI changes.  The developers have been using computers a very long time and are not interested in chasing modern UI fads unless a good reason can be shown _why_ the UI should be changed.  For example, take a poll, or even better raise an issue asking for feedback.  This way we have hard data showing that there is interest instead of assuming that a change is automatically "better".

Trivial changes such as updating art are much easier to accept than rewriting the UI moving buttons around.

When discussing topics focus on the problem and potential solutions and not people.  Please keep things professional.

Lasty, there are many reasons why we may reject a feature or PR.  This does NOT mean we aren't interested; it just means that the feature or PR doesn't meet the developer's expectations and project goals _at this time._

Thanks again for your interest in wanting to make AppleWin better.
