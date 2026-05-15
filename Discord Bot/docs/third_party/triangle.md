# Triangle.js

Triangle.js is used as an optional sidecar dependency for TETR.IO room automation.

Repository:

```text
https://github.com/halp1/triangle
```

Documentation:

```text
https://triangle.haelp.dev/
```

NPM package:

```text
@haelp/teto
```

License:

```text
MIT
```

Committed integration location:

```text
tools/triangle-bridge
```

The bridge consumes Triangle.js through the `@haelp/teto` npm package. A local checkout of `https://github.com/halp1/triangle` may be used as a development reference, but it is not required for the C++ bot build.

Preserve Triangle.js' MIT license notice when redistributing the npm package, a vendored copy, or source-derived code.

## TETR.IO API Warning

Triangle.js is not officially supported or endorsed by TETR.IO. Its documentation warns that TETR.IO main game API use is only permitted with an official bot account and may break when TETR.IO changes.

For this project, Triangle.js integration must remain:

- optional,
- disabled by default,
- trusted-deployment only,
- backed by manual room creation fallback.
