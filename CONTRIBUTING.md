## Thank you for making WLED better!

Here are a few suggestions to make it easier for you to contribute!

### Describe your PR

Please add a description of your proposed code changes. It does not need to be an exhaustive essay, however a PR with no description or just a few words might not get accepted, simply because very basic information is missing.

A good description helps us to review and understand your proposed changes. For example, you could say a few words about
* what you try to achieve (new feature, fixing a bug, refactoring, security enhancements, etc.)
* how your code works (short technical summary - focus on important aspects that might not be obvious when reading the code)
* testing you performed, known limitations, open ends you possibly could not solve.
* any areas where you like to get help from an experienced maintainer (yes WLED has become big 😉)

### Target branch for pull requests

Please make all PRs against the `main` branch.

### Updating your code
While the PR is open - and under review by maintainers - you may be asked to modify your PR source code.
You can simply update your own branch, and push changes in response to reviewer recommendations. 
Github will pick up the changes so your PR stays up-to-date.

> [!CAUTION]
> Do not use "force-push" while your PR is open!
> It has many subtle and unexpected consequences on our github reposistory.
> For example, we regularly lost review comments when the PR author force-pushes code changes. So, pretty please, do not force-push.


You can find a collection of very useful tips and tricks here: https://github.com/wled-dev/WLED/wiki/How-to-properly-submit-a-PR


### Code style

When in doubt, it is easiest to replicate the code style you find in the files you want to edit :)
Below are the guidelines we use in the WLED repository.

#### Indentation

We use tabs for Indentation in Web files (.html/.css/.js) and spaces (2 per indentation level) for all other files.  
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

#### Comments

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
