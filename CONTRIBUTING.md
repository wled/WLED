# Thank you for making WLED better!

WLED is a community-driven project, and every contribution matters! We appreciate your time and effort.

Our maintainers are here for two things: **helping you** improve your code, and **keeping WLED** lean, efficient, and maintainable. 
We'll work with you to refine your contribution, but we'll also push back if something might create technical debt or add features without clear value. Don't take it personally - we're just protecting WLED's architecture while helping your contribution succeed!

## Opening Issues & Feature Requests

Whether you're reporting a bug or suggesting a new feature, please lead with **what you want to achieve or what's bothering you, in plain language** - not with a specific code path, root cause, or proposed source change. For example, "the LEDs flicker when I use transition X at low brightness" or "I'd like to control WLED without needing an app" tells us what matters, before any technical detail.

Maintainers are usually in a better position to map your objective onto a good technical solution and weigh the trade-offs (memory, performance, maintainability). If you already have a technical idea or a fix in mind, that's welcome too - just add it as additional context after describing the underlying goal, so we understand *why* before we evaluate *how*.

## Getting Started

Here are a few suggestions to make it easier for you to contribute:

### Important Developer Infos
* [Project Structure, Files and Directories](AGENTS.md#project-structure) (in our AI instructions)
* [Instructions for creating usermods](AGENTS.md#usermod-pattern) (in our AI instructions)
* KB: [Compiling WLED](https://kno.wled.ge/advanced/compiling-wled/) - slightly outdated but still helpful :-)
* Arduino IDE is not supported any more. Use VSCode with the PlatformIO extension. 
* [Compiling in VSCode/Platformio](https://github.com/wled/WLED-Docs/issues/161) - modern way without command line or platformio.ini changes.
* If you add a new feature, consider making a PR to [``wled-docs``](https://github.com/wled/WLED-Docs) for updating our official documentation.

### PR from a branch in your own fork
Start your pull request (PR) in a branch of your own fork. Don't make a PR directly from your main branch.
This lets you update your PR if needed, while you can work on other tasks in 'main' or in other branches.

> [!TIP]
>   **The easiest way to start your first PR**
>   When viewing a file in `wled/WLED`, click on the "pen" icon and start making changes.
>   When you choose to 'Commit changes', GitHub will automatically create a PR from your fork.
>   
>   <img width="295" height="134" alt="image: fork and edit" src="https://github.com/user-attachments/assets/f0dc7567-edcb-4409-a530-cd621ae9661f" />

### Target branch for pull requests

> [!IMPORTANT]
> Please make all PRs against the `main` branch.

### Describing your PR

Please add a description of your proposed code changes. 
A PR with no description or just a few words might not get accepted, simply because very basic information is missing.
No need to write an essay!

A good description helps us to review and understand your proposed changes. For example, you could say a few words about
* What you try to achieve (new feature, fixing a bug, refactoring, security enhancements, etc.)
* How your code works (short technical summary - focus on important aspects that might not be obvious when reading the code)
* Testing you performed, known limitations, anything you couldn't quite solve.
* Let us know if you'd like guidance from a maintainer (WLED is a big project 😉)

> [!TIP]
> **Say what you actually know, not what sounds reassuring**
> Phrases like "I steered the whole process", "the design decisions are mine", or "I've validated the changes with my hardware" are meant to reassure reviewers - but they can quietly become weasel words that paper over "I don't fully understand this part of the WLED codebase".
> If that's the honest situation, please just say so directly, e.g. "I'm not fully sure why this fixes it, but it resolved the flicker on my ESP32 + WS2812B setup, and here's what I *do* understand about the change: ...".  
> We'd much rather hear that than a confident-sounding reassurance - it tells us exactly how much scrutiny to apply, and saves everyone time.

### Testing Your Changes

Before submitting:

- ✅ Does it compile?
- ✅ Does your feature/fix actually work?
- ✅ Did you break anything else?
- ✅ Tested on actual hardware if possible?

Mention your testing in the PR description (e.g., "Tested on ESP32 + WS2812B").

> [!TIP]
> If an AI suggested the fix, "should work" based on reading the code isn't the same as testing it.
> Be clear about which one you did, e.g. "confirmed on hardware: LEDs no longer flicker" vs. "I believe this should work based on reading the code, not yet tested on hardware".
> Both are useful information for reviewers - just don't present a guess as a confirmed result.

## During Review

We're all volunteers, so reviews can take some time (longer during busy times). 
Don't worry - we haven't forgotten you! Feel free to ping after a week if there's no activity.

### Updating your code
While the PR is open, you can keep updating your branch - just push more commits! GitHub will automatically update your PR. 

You don't need to squash commits or clean up history - we'll handle that when merging.

> [!CAUTION] 
> Do not use "force-push" while your PR is open!
> It has many subtle and unexpected consequences on our GitHub repository.
> For example, we regularly lose review comments when the PR author force-pushes code changes. Our review bot (coderabbit) may become unable to properly track changes, it gets confused or stops responding to questions.
> So, pretty please, do not force-push.

> [!TIP]
> Use [cherry-picking](https://docs.github.com/en/desktop/managing-commits/cherry-picking-a-commit-in-github-desktop) to copy commits from one branch to another.


### Responding to Reviews

When we ask for changes:

- **Add new commits** - please don't amend or force-push
- **Reply in the PR** - let us know when you've addressed comments
- **Ask questions** - if something's unclear, just ask!
- **Be patient** - we're all volunteers here 😊

You can reference feedback in commit messages:
> ```text
> Fix naming per @Aircoookie's suggestion
> ```

### Dealing with Merge Conflicts

Got conflicts with `main`? No worries - here's how to fix them:

**Using GitHub Desktop** (easier for beginners):

1. Click **Fetch origin**, then **Pull origin**
2. If conflicts exist, GitHub Desktop will warn you - click **View conflicts**
3. Open the conflicted files in your editor (VS Code, etc.)
4. Remove the conflict markers (`<<<<<<<`, `=======`, `>>>>>>>`) and keep the correct code
5. Save the files
6. Back in GitHub Desktop, commit the merge (it'll suggest a message)
7. Click **Push origin**

**Using command line**:

   ```bash
   git fetch origin
   git merge origin/main
   # Fix conflicts in your editor
   git add .
   git commit
   git push
   ```

Either way works fine - pick what you're comfortable with! Merging is simpler than rebasing and keeps everything connected.

#### When you MUST rebase (really rare!)

Sometimes you might hit merge conflicts with `main` that are harder to solve. Here's what to try:

1. **Merge instead of rebase** (safest option):
   ```bash
   git fetch origin
   git merge origin/main
   git push
   ```
   Keeps review comments attached and CI results visible!

2. **Use cherry-picking** to copy commits between branches without rewriting history - [here's how](https://docs.github.com/en/desktop/managing-commits/cherry-picking-a-commit-in-github-desktop).

3. **If all else fails, use `--force-with-lease`** (not plain `--force`):
   ```bash
   git rebase origin/main
   git push --force-with-lease
   ```
   Then **leave a comment** explaining why you had to force-push, and be ready to re-address some feedback.

### Additional Resources
Want to know more? Check out:
- 📚 [GitHub Desktop documentation](https://docs.github.com/en/desktop) - if you prefer GUI tools

## After Approval
Once approved, a maintainer will merge your PR (possibly squashing commits). 
Your contribution will be in the next WLED release - thank you! 🎉


## Coding Guidelines

### Source Code from an AI agent or bot
> [!TIP]
> It's OK if you took help from an AI for writing your source code. 
> AI tools can be very helpful, but as the contributor, **you're responsible for the code**. We expect that you've reviewed AI code thoroughly before opening a PR.

Why we ask for this: our maintainers are volunteers with limited time. A PR where the author can't explain their own code, or a bug fix whose "root cause" turns out to be wrong once someone actually looks, costs real review time - and it's genuinely no fun to reject after you (or your agent) put in the effort. These guidelines exist to help your contribution succeed on the first pass, not to gatekeep AI use.

> [!CAUTION]
> Purely "vibe-coded" PRs - where the author hasn't reviewed, tested, and understood the source code changes - **can and will be rejected** by maintainers. This isn't about being unfriendly to AI use; it's that nobody, including a well-meaning maintainer, can safely merge and maintain code that its own author can't explain or vouch for. Please take the time to review and understand what you're submitting, so we don't have to say no.

* Make sure you really understand the AI-generated code, don't just accept it because it "seems to work".
* Don't let the AI change existing code without double-checking by you as the contributor. Often, the result will not be complete. For example, previous source code comments may be lost.
* Remember that AI is still "Often-Wrong" ;-)
* If you don't feel confident using English, you can use AI for translating code comments and descriptions into English. AI bots are very good at understanding language. However, always check if the results are correct. The translation might still have wrong technical terms, or errors in some details.

> [!IMPORTANT]
> Fully or partially AI coded PRs MUST be declared clearly in the PR description, in addition to comment markers in the source code. This helps reviewers to set expectations and focus on typical AI mistakes. It also helps contributors to avoid frustration. 

#### Can you explain it?

Reviewers may ask you direct questions about your change, or about a root cause you've claimed. Please expect to answer these yourself, in your own words - not by pasting your AI's reply back verbatim without checking it first. If you can't currently answer a question, it's fine to say so and take the time to dig in; that's a much better outcome for everyone than a confident-sounding guess.

This applies especially to root-cause claims. Before writing "the bug is caused by X", please verify it against the actual code path (and, ideally, a log or a reproduction) rather than asserting it purely because an AI sounded sure. A quick note on how you verified it goes a long way, e.g. "confirmed by adding a debug print at `strip.cpp:123` - the value was indeed wrapping around". If you haven't verified it yet, say that too ("AI suggested this cause, not yet confirmed") - it's honest and saves everyone time.

#### Keep PRs scoped

Large, multi-subsystem "refactor" or "architecture" PRs - especially unsolicited ones - are hard to review and easy to get wrong, whether AI-assisted or not. If you (or your AI agent) find that fixing something seems to require touching many unrelated files or restructuring a subsystem, please open an issue or start a discussion ([discord](https://discord.gg/QAh7wJHrRM) or [discourse](https://wled.discourse.group)) first, so maintainers can weigh in on direction before a lot of work goes into it. This isn't a hard rule - small, well-explained refactors are always welcome - it's just to avoid a large PR falling apart under review because the direction wasn't agreed on first.

> [!NOTE]
> "Keep it scoped" cuts both ways. It's not a license to dodge a needed core-level discussion by bolting a board- or usermod-specific workaround onto the side instead - weak hooks, monkey-patched dependencies, or pre-build script patches can look "minimal" in lines-changed while actually avoiding the harder conversation about where a feature belongs. If your use case genuinely needs a new core capability or interface, please say so and propose it openly, rather than working around the core to keep the diff small.

#### Best Practice with AI

AI tools are powerful but "often wrong" - your judgment is essential! 😊

- ✅ **Understand the code** - As the person contributing to WLED, make sure you understand exactly what the AI-generated source code does
- ✅ **Review carefully** - AI can lose comments, introduce bugs, or make unnecessary changes
- ✅ **Be transparent** - Add comments `// AI: below section was generated by an AI` ... `// AI: end` around larger chunks
- ✅ **Use AI for translation** - AI is great for translating comments to English (but verify technical terms!)
- ✅ **Keep it scoped** - Don't let an agent "helpfully" expand a small fix into a big refactor without asking you first

#### AI disclosure block for your PR description

For AI-assisted changes, feel free to paste a short block like this into your PR description. This helps reviewers to set the right expectations quickly - no need to write more than a few lines:

```text
AI assistance: yes
What the AI drafted: <e.g. the initial fix in usermod X, refactoring of function Y>
What I verified/tested myself: <e.g. read through the diff, tested on ESP32 + WS2812B, confirmed the root cause with a debug print>
Untested / unsure about: <anything you didn't get to verify>
```

### Code style

Don't stress too much about style! When in doubt, just match the style in the files you're editing. 😊

Our review bot (coderabbit) has learned lots of detailed guides and hints - it will suggest them automatically when you submit a PR for review.
If you are curious, these are the detailed guides:
* [C++ Coding](docs/cpp.instructions.md)
* [WebUi: HTML, JS, CSS](docs/web.instructions.md)

Below are the main rules used in the WLED repository:

#### Indentation

We use tabs for indentation in Web files (.html/.css/.js) and spaces (2 per indentation level) for all other files.  
You are all set if you have enabled `Editor: Detect Indentation` in VS Code.

#### Blocks

Whether the opening bracket of e.g. an `if` block is in the same line as the condition or in a separate line is up to your discretion. If there is only one statement, leaving out block brackets is acceptable.

Good:  
```cpp
if (a == b) {
  doStuff(a);
}
```

```cpp
if (a == b) doStuff(a);
```

Also acceptable (though the first style is usually easier to read):
```cpp
if (a == b)
{
  doStuff(a);
}
```


There should always be a space between a keyword and its condition and between the condition and brace.  
Within the condition, no space should be between the parenthesis and variables.  
Spaces between variables and operators are up to the authors discretion.
There should be no space between function names and their argument parenthesis.

Good:  
```cpp
if (a == b) {
  doStuff(a);
}
```

Not good:  
```cpp
if( a==b ){
  doStuff ( a);
}
```

#### Comments

Comments should have a space between the delimiting characters (e.g. `//`) and the comment text.
We're gradually adopting this style - don't worry if you see older code without spaces!

Good:  
```cpp
// This is a short inline comment.

/* 
 * This is a longer comment
 * wrapping over multiple lines,
 * used in WLED for file headers and function explanations
 */
```
```css
/* This is a CSS inline comment */
```
```html
<!-- This is an HTML comment -->
```

There is no hard character limit for a comment within a line,
though as a rule of thumb consider wrapping after 120 characters.
Inline comments are OK if they describe that line only and are not exceedingly wide.
