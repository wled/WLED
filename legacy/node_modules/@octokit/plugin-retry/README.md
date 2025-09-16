# plugin-retry.js

> Retries requests for server 4xx/5xx responses except `400`, `401`, `403`, `404`, `410`, `422`, and `451`.

[![@latest](https://img.shields.io/npm/v/@octokit/plugin-retry.svg)](https://www.npmjs.com/package/@octokit/plugin-retry)
[![Build Status](https://github.com/octokit/plugin-retry.js/workflows/Test/badge.svg)](https://github.com/octokit/plugin-retry.js/actions?workflow=Test)

## Usage

<table>
<tbody valign=top align=left>
<tr><th>
Browsers
</th><td width=100%>

Load `@octokit/plugin-retry` and [`@octokit/core`](https://github.com/octokit/core.js) (or core-compatible module) directly from [esm.sh](https://esm.sh)

```html
<script type="module">
  import { Octokit } from "https://esm.sh/@octokit/core";
  import { retry } from "https://esm.sh/@octokit/plugin-retry";
</script>
```

</td></tr>
<tr><th>
Node
</th><td>

Install with `npm install @octokit/core @octokit/plugin-retry`. Optionally replace `@octokit/core` with a core-compatible module

```js
import { Octokit } from "@octokit/core";
import { retry } from "@octokit/plugin-retry";
```

</td></tr>
</tbody>
</table>

> [!IMPORTANT]
> As we use [conditional exports](https://nodejs.org/api/packages.html#conditional-exports), you will need to adapt your `tsconfig.json` by setting `"moduleResolution": "node16", "module": "node16"`.
>
> See the TypeScript docs on [package.json "exports"](https://www.typescriptlang.org/docs/handbook/modules/reference.html#packagejson-exports).<br>
> See this [helpful guide on transitioning to ESM](https://gist.github.com/sindresorhus/a39789f98801d908bbc7ff3ecc99d99c) from [@sindresorhus](https://github.com/sindresorhus)

```js
const MyOctokit = Octokit.plugin(retry);
const octokit = new MyOctokit({ auth: "secret123" });

// retries request up to 3 times in case of a 500 response
octokit.request("/").catch((error) => {
  if (error.request.request.retryCount) {
    console.log(
      `request failed after ${error.request.request.retryCount} retries`,
    );
  }

  console.error(error);
});
```

To override the default `doNotRetry` list:

```js
const octokit = new MyOctokit({
  auth: "secret123",
  retry: {
    doNotRetry: [
      /* List of HTTP 4xx/5xx status codes */
    ],
  },
});
```

To override the number of retries:

```js
const octokit = new MyOctokit({
  auth: "secret123",
  request: { retries: 1 },
});
```

You can manually ask for retries for any request by passing `{ request: { retries: numRetries, retryAfter: delayInSeconds }}`. Note that the `doNotRetry` option from the constructor is ignored in this case, requests will be retried no matter their response code.

```js
octokit
  .request("/", { request: { retries: 1, retryAfter: 1 } })
  .catch((error) => {
    if (error.request.request.retryCount) {
      console.log(
        `request failed after ${error.request.request.retryCount} retries`,
      );
    }

    console.error(error);
  });
```

Pass `{ retry: { enabled: false } }` to disable this plugin.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md)

## License

[MIT](LICENSE)
