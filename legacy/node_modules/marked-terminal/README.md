# marked-terminal

> Custom Renderer for [marked](https://github.com/chjj/marked)
> allowing for printing Markdown to the Terminal. Supports pretty tables, syntax
> highlighting for javascript, and overriding all colors and styles.

Could for instance be used to print usage information.

[![build](https://github.com/mikaelbr/marked-terminal/actions/workflows/ci.yml/badge.svg)](https://github.com/mikaelbr/marked-terminal/actions/workflows/ci.yml) [![npm marked-terminal](https://img.shields.io/npm/v/marked-terminal.svg)](https://www.npmjs.com/package/marked-terminal)

## Install

```sh
npm install marked marked-terminal
```

## Example

```javascript
import { marked } from 'marked';
import { markedTerminal } from 'marked-terminal';

marked.use(markedTerminal([options][, highlightOptions]));

marked.parse('# Hello \n This is **markdown** printed in the `terminal`');
```

### Using older versions

```javascript
const marked = require('marked');
const TerminalRenderer = require('marked-terminal');

marked.setOptions({
  // Define custom renderer
  renderer: new TerminalRenderer()
});

// Show the parsed data
console.log(
  marked('# Hello \n This is **markdown** printed in the `terminal`')
);
```

This will produce the following:

![Screenshot of marked-terminal](./screenshot.png)

### Syntax Highlighting

Also have support for syntax highlighting using [cli-highlight](https://github.com/felixfbecker/cli-highlight).
You can override highlighting defaults by passing in settings as the second argument for TerminalRenderer.

Having the following markdown input:

<pre>
```js
var foo = function(bar) {
  console.log(bar);
};
foo('Hello');
```
</pre>

...we will convert it into terminal format:

```javascript
// Show the parsed data
console.log(marked(exampleSource));
```

This will produce the following:

![Screenshot of marked-terminal](./screenshot2.png)

## API

Constructur: `new TerminalRenderer([options][, highlightOptions])`

### `options`

Optional
Used to override default styling.

Default values are:

```javascript
var defaultOptions = {
  // Colors
  code: chalk.yellow,
  blockquote: chalk.gray.italic,
  html: chalk.gray,
  heading: chalk.green.bold,
  firstHeading: chalk.magenta.underline.bold,
  hr: chalk.reset,
  listitem: chalk.reset,
  table: chalk.reset,
  paragraph: chalk.reset,
  strong: chalk.bold,
  em: chalk.italic,
  codespan: chalk.yellow,
  del: chalk.dim.gray.strikethrough,
  link: chalk.blue,
  href: chalk.blue.underline,

  // Formats the bulletpoints and numbers for lists
  list: function (body, ordered) {/* ... */},

  // Reflow and print-out width
  width: 80, // only applicable when reflow is true
  reflowText: false,

  // Should it prefix headers?
  showSectionPrefix: true,

  // Whether or not to undo marked escaping
  // of enitities (" -> &quot; etc)
  unescape: true,

  // Whether or not to show emojis
  emoji: true,

  // Options passed to cli-table3
  tableOptions: {},

  // The size of tabs in number of spaces or as tab characters
  tab: 3 // examples: 4, 2, \t, \t\t

  image: function (href, title, text) {} // function for overriding the default image handling.
};
```

#### Example of overriding defaults

```javascript
marked.setOptions({
  renderer: new TerminalRenderer({
    codespan: chalk.underline.magenta
  })
});
```

### `highlightOptions`

Options passed into [cli-highlight](https://github.com/felixfbecker/cli-highlight). See readme there to see what options to pass.

See [more examples](./example/)

## Related

- [ink-markdown](https://github.com/cameronhunter/ink-markdown) - Markdown component for Ink
