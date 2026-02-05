# Thank you for making WLED better!

Here are a few suggestions to make it easier for you to contribute!

## PR from a branch in your own fork
Start your pull request (PR) in a branch of your own fork. Don't make a PR directly from your main branch.
This lets you update your PR if needed, while you can work on other tasks in 'main' or in other branches.

> [!TIP]
>   **The easiest way to start your first PR**
>   When viewing a file in `wled/WLED`, click on the "pen" icon and start making changes.
>   When you chose to 'Commit changes', GitHub will automatically create a PR from your fork.
>   
>   <img width="295" height="134" alt="image: fork and edit" src="https://github.com/user-attachments/assets/f0dc7567-edcb-4409-a530-cd621ae9661f" />


### Target branch for pull requests

Please make all PRs against the `main` branch.

### Describing your PR

Please add a description of your proposed code changes. It does not need to be an exhaustive essay, however a PR with no description or just a few words might not get accepted, simply because very basic information is missing.

A good description helps us to review and understand your proposed changes. For example, you could say a few words about
* what you try to achieve (new feature, fixing a bug, refactoring, security enhancements, etc.)
* how your code works (short technical summary - focus on important aspects that might not be obvious when reading the code)
* testing you performed, known limitations, open ends you possibly could not solve.
* any areas where you like to get help from an experienced maintainer (yes WLED has become big 😉)

### Testing Your Changes

Before submitting:

- ✅ Does it compile?
- ✅ Does your feature/fix actually work?
- ✅ Did you break anything else?
- ✅ Tested on actual hardware if possible?

Mention your testing in the PR description (e.g., "Tested on ESP32 + WS2812B").


## Updating your code
While the PR is open, you can keep updating your branch - just push more commits! GitHub will automatically update your PR. 

You don't need to squash commits or clean up history - we'll handle that when merging.

> [!CAUTION] 
> Do not use "force-push" while your PR is open!
> It has many subtle and unexpected consequences on our GitHub repository.
> For example, we regularly lost review comments when the PR author force-pushes code changes. Our review bot (coderabbit) may become unable to properly track changes, it gets confused or stops responding to questions.
> So, pretty please, do not force-push.

> [!TIP]
> use [cherry-picking](https://docs.github.com/en/desktop/managing-commits/cherry-picking-a-commit-in-github-desktop) to copy commits from one branch to another.


### Responding to Reviews

When we ask for changes:

- **Add new commits** - please don't amend or force-push
- **Reply in the PR** - let us know when you've addressed comments
- **Ask questions** - if something's unclear, just ask!
- **Be patient** - we're all volunteers here 😊

You can reference feedback in commit messages:
> ```text
> Fix naming per `@Aircoookie`'s suggestion
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

### Additional Ressources
You can find a collection of very useful tips and tricks here: https://github.com/wled-dev/WLED/wiki/How-to-properly-submit-a-PR


## Source Code from an AI agent or bot
> [!IMPORTANT]
> Its OK if you took help from an AI for writing your source code. 
>
> However, we expect a few things from you as the person making a contribution to WLED:
* Make sure you really understand the code suggested by the AI, and don't just accept it because it "seems to work".
* Don't let the AI change existing code without double-checking by you as the contributor. Often, the result will not be complete. For example, previous source code comments may be lost.
* Remember that AI are still "Often-Wrong" ;-)
* If you don't feel very confident using English, you can use AI for translating code comments and descriptions into English. AI bots are very good at understanding language. However, always check if the results is correct. The translation might still have wrong technical terms, or errors in some details.

#### best practice with AI:
  * As the person who contributes source code to WLED, make sure you understand exactly what the AI generated code does
  * best practice: add a comment like ``'// below section of my code was generated by an AI``, when larger parts of your source code were not written by you personally.
  * always review translations and code comments for correctness
  * always review AI generated source code
  * If the AI has rewritten existing code, check that the change is necessary and that nothing has been lost or broken. Also check that previous code comments are still intact.


## Code style

When in doubt, it is easiest to replicate the code style you find in the files you want to edit :)
Below are the guidelines we use in the WLED repository.

### Indentation

We use tabs for Indentation in Web files (.html/.css/.js) and spaces (2 per indentation level) for all other files.  
You are all set if you have enabled `Editor: Detect Indentation` in VS Code.

### Blocks

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

Acceptable - however the first variant is usually easier to read:
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

### Comments

Comments should have a space between the delimiting characters (e.g. `//`) and the comment text.
Note: This is a recent change, the majority of the codebase still has comments without spaces.

Good:  
```
// This is a comment.

/* This is a CSS inline comment */

/* 
 * This is a comment
 * wrapping over multiple lines,
 * used in WLED for file headers and function explanations
 */

<!-- This is an HTML comment -->
```

There is no hard character limit for a comment within a line,
though as a rule of thumb consider wrapping after 120 characters.
Inline comments are OK if they describe that line only and are not exceedingly wide.
