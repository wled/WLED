# Usermods

This folder contains usermods, optional WLED components maintained by the core WLED team and some legacy modules with separate maintainers.  Usermods are self-contained modules that add functionality without modifying core source files.

## Writing your own usermod

Start from the official example repository — click **Use this template** on GitHub to create your own copy:

**[github.com/wled/wled-usermod-example](https://github.com/wled/wled-usermod-example)**

It contains a fully annotated implementation and a `library.json` template. Keep your usermod in its own repository and reference it from your WLED build via `custom_usermods` — no changes to the WLED source tree needed.

For the complete guide see **[kno.wled.ge/advanced/custom-features](https://kno.wled.ge/advanced/custom-features/)**, covering:

- Enabling usermods via `custom_usermods` in `platformio_override.ini`
- Local development with `symlink://` references
- Sharing via git URL
- `library.json` structure and the required `"libArchive": false` setting
- All lifecycle methods (`setup`, `loop`, `addToConfig`, `readFromConfig`, etc.)
- Adding custom LED effects via usermod

Once your usermod is ready, add it to the [Community Usermods index](https://kno.wled.ge/advanced/community-usermods/) and tag your repository with the [`wled-usermod`](https://github.com/topics/wled-usermod) GitHub topic so others can find it.

## Contributing a usermod to this folder

The preferred approach is an independent repository (see above).  If you strongly believe that your module adds a commonly missing feature that would be useful in most WLED installations, and maintenance can be managed by the core WLED team, you can suggest it for inclusion here:

- Create a subfolder with a descriptive name
- Include a `library.json` and your source files
- Add a `README.md` describing what the mod does and any wiring or configuration required
- Open a pull request on the WLED repo

The bar for inclusion in the main WLED repository is quite high.  We encourage you to consider maintaining the module in your own repository for a period of time first.
