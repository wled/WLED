## Thank you for making WLED better!

Here are a few suggestions to make it easier for you to contribute!

### Code style

When in doubt, it is easiest to replicate the code style you find in the files you want to edit :)
Below are the guidelines we use in the WLED repository.

#### Indentation

We use tabs for Indentation in Web files (.html/.css/.js) and spaces (2 per indentation level) for all other files.  
You are all set if you have enabled `Editor: Detect Indentation` in VS Code.

#### Blocks

Whether the opening bracket of e.g. an `if` block is in the same line as the condition or in a separate line is up to your discretion. If there is only one statement, leaving out block braches is acceptable.

Good:  
```cpp
if (a == b) {
  doStuff(a);
}
```

```cpp
if (a == b)
{
  doStuff(a);
}
```

```cpp
if (a == b) doStuff(a);
```

There should always be a space between a keyword and its condition and between the condition and brace.  
Within the condition, no space should be between the paranthesis and variables.  
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

There is no set character limit for a comment within a line,
though as a rule of thumb you should wrap your comment if it exceeds the width of your editor window.  
Inline comments are OK if they describe that line only and are not exceedingly wide.