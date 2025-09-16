export default function isPrerelease({ type, main, prerelease }) {
  if (prerelease === false) {
    return false;
  }
  return (
    type === "prerelease" ||
    (type === "release" && !main) ||
    typeof prerelease == "string" ||
    prerelease === true
  );
}
