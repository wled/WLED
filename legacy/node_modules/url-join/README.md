Join all arguments together and normalize the resulting URL.

## Install

```bash
npm install url-join
```

If you want to use it directly in a browser use a CDN like [Skypack](https://www.skypack.dev/view/url-join).

## Usage

```javascript
import urlJoin from 'url-join';

const fullUrl = urlJoin('http://www.google.com', 'a', '/b/cd', '?foo=123');

console.log(fullUrl);
```

Prints:

```
'http://www.google.com/a/b/cd?foo=123'
```

## License

MIT
