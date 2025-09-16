var __classPrivateFieldGet = (this && this.__classPrivateFieldGet) || function (receiver, state, kind, f) {
    if (kind === "a" && !f) throw new TypeError("Private accessor was defined without a getter");
    if (typeof state === "function" ? receiver !== state || !f : !state.has(receiver)) throw new TypeError("Cannot read private member from an object whose class did not declare it");
    return kind === "m" ? f : kind === "a" ? f.call(receiver) : f ? f.value : state.get(receiver);
};
var _Mime_extensionToType, _Mime_typeToExtension, _Mime_typeToExtensions;
class Mime {
    constructor(...args) {
        _Mime_extensionToType.set(this, new Map());
        _Mime_typeToExtension.set(this, new Map());
        _Mime_typeToExtensions.set(this, new Map());
        for (const arg of args) {
            this.define(arg);
        }
    }
    define(typeMap, force = false) {
        for (let [type, extensions] of Object.entries(typeMap)) {
            type = type.toLowerCase();
            extensions = extensions.map((ext) => ext.toLowerCase());
            if (!__classPrivateFieldGet(this, _Mime_typeToExtensions, "f").has(type)) {
                __classPrivateFieldGet(this, _Mime_typeToExtensions, "f").set(type, new Set());
            }
            const allExtensions = __classPrivateFieldGet(this, _Mime_typeToExtensions, "f").get(type);
            let first = true;
            for (let extension of extensions) {
                const starred = extension.startsWith('*');
                extension = starred ? extension.slice(1) : extension;
                allExtensions?.add(extension);
                if (first) {
                    __classPrivateFieldGet(this, _Mime_typeToExtension, "f").set(type, extension);
                }
                first = false;
                if (starred)
                    continue;
                const currentType = __classPrivateFieldGet(this, _Mime_extensionToType, "f").get(extension);
                if (currentType && currentType != type && !force) {
                    throw new Error(`"${type} -> ${extension}" conflicts with "${currentType} -> ${extension}". Pass \`force=true\` to override this definition.`);
                }
                __classPrivateFieldGet(this, _Mime_extensionToType, "f").set(extension, type);
            }
        }
        return this;
    }
    getType(path) {
        if (typeof path !== 'string')
            return null;
        const last = path.replace(/^.*[/\\]/, '').toLowerCase();
        const ext = last.replace(/^.*\./, '').toLowerCase();
        const hasPath = last.length < path.length;
        const hasDot = ext.length < last.length - 1;
        if (!hasDot && hasPath)
            return null;
        return __classPrivateFieldGet(this, _Mime_extensionToType, "f").get(ext) ?? null;
    }
    getExtension(type) {
        if (typeof type !== 'string')
            return null;
        type = type?.split?.(';')[0];
        return ((type && __classPrivateFieldGet(this, _Mime_typeToExtension, "f").get(type.trim().toLowerCase())) ?? null);
    }
    getAllExtensions(type) {
        if (typeof type !== 'string')
            return null;
        return __classPrivateFieldGet(this, _Mime_typeToExtensions, "f").get(type.toLowerCase()) ?? null;
    }
    _freeze() {
        this.define = () => {
            throw new Error('define() not allowed for built-in Mime objects. See https://github.com/broofa/mime/blob/main/README.md#custom-mime-instances');
        };
        Object.freeze(this);
        for (const extensions of __classPrivateFieldGet(this, _Mime_typeToExtensions, "f").values()) {
            Object.freeze(extensions);
        }
        return this;
    }
    _getTestState() {
        return {
            types: __classPrivateFieldGet(this, _Mime_extensionToType, "f"),
            extensions: __classPrivateFieldGet(this, _Mime_typeToExtension, "f"),
        };
    }
}
_Mime_extensionToType = new WeakMap(), _Mime_typeToExtension = new WeakMap(), _Mime_typeToExtensions = new WeakMap();
export default Mime;
