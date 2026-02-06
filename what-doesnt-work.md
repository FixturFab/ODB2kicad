# What Doesn't Work

## Stage 1: Native Build Foundation

### Minor Issues (non-blocking):
1. **API schema warning**: `Error: schema file '/usr/local/share/kicad/schemas/api.v1.schema.json' not found` - appears on every kicad-cli invocation but doesn't affect functionality. Would need to install KiCad's schema files to the expected system path to resolve.

2. **kiface path mismatch**: kicad-cli expects `_pcbnew.kiface` in the same directory as the binary (`kicad/`), but the build places it in `pcbnew/`. Required manual symlink to resolve. This is a build system quirk when running from the build directory without `make install`.

3. **Git describe fails**: `git describe returned error 128: fatal: No names found, cannot describe anything` during cmake configure. This is because the shallow clone (`--depth 1`) has no tags. Results in version showing as "9.99.0" instead of a tagged release version. Non-blocking.

### No blocking issues. All Stage 1 success criteria are met.

## Stage 2: Emscripten Toolchain Validation

### Minor Issues (non-blocking):
1. **EXPORT_ES6 requires .mjs extension**: When compiling with `-s EXPORT_ES6=1`, the output must use a `.mjs` extension (not `.js`) for Node.js to recognize it as an ES module. With a `.js` extension, Node 18 fails with `SyntaxError: Cannot use 'import.meta' outside a module`. Alternatively, a `package.json` with `"type": "module"` in the directory would also work, but `.mjs` is simpler.

### No blocking issues. All Stage 2 success criteria are met.
