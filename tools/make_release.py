"""Builds dist/LoS2Control.<version>.zip: the FOMOD installer of LoS2 Control.

    python tools/make_release.py <version> <new DLL> <older DLL> <previous release zip>

The new DLL is the alandtse NG build (ng-build/build/release/out/LoS2Control), the older one comes from plugin/build.cmd.
The LoS II addon files (addon/: LoS2-DLL.esp and the two Light Placer json files) and fomod/ModuleConfig.xml are taken from the previous release zip, because
they are not in this repository. Everything else comes from the repository. Nothing is uploaded.
"""
import hashlib
import io
import os
import sys
import xml.dom.minidom
import zipfile

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
version, dll_new, dll_old, prev = sys.argv[1:5]
for p in (dll_new, dll_old, prev):
    assert os.path.isfile(p), f"missing: {p}"
out_dir = os.path.join(root, "dist")
os.makedirs(out_dir, exist_ok=True)
out = os.path.join(out_dir, f"LoS2Control.{version}.zip")


def rd(p):
    return open(os.path.join(root, p), "rb").read()


files = {}  # path in the zip -> bytes
with zipfile.ZipFile(prev) as z:
    for n in z.namelist():
        if n.startswith("addon/") and not n.endswith("/"):
            files[n] = z.read(n)
    files["fomod/ModuleConfig.xml"] = z.read("fomod/ModuleConfig.xml")
    info = z.read("fomod/info.xml").decode("utf-8")
import re
info = re.sub(r"<Version>[^<]*</Version>", f"<Version>{version}</Version>", info)
xml.dom.minidom.parseString(info)
xml.dom.minidom.parseString(files["fomod/ModuleConfig.xml"])
files["fomod/info.xml"] = info.encode("utf-8")

for name in ("LICENSE", "NOTICE", "README.md"):
    files[name] = rd(name)
files["core/Interface/LoS2Control/los2_logo.png"] = rd("package/Interface/LoS2Control/los2_logo.png")
files["core/SKSE/Plugins/LoS2Control/lang/_template.json"] = rd("package/SKSE/Plugins/LoS2Control/lang/_template.json")
files["core/SKSE/Plugins/LoS2Control_Weathers.txt"] = rd("package/SKSE/Plugins/LoS2Control_Weathers.txt")
files["dll-ng/SKSE/Plugins/LoS2Control.dll"] = open(dll_new, "rb").read()
files["dll-legacy/SKSE/Plugins/LoS2Control.dll"] = open(dll_old, "rb").read()
assert not any("japanese" in n.lower() for n in files), "a translation draft must not ship"

if os.path.exists(out):
    os.remove(out)
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
    for n in sorted(files):
        z.writestr(n, files[n])
with zipfile.ZipFile(out) as z:
    for i in z.infolist():
        print(f"{i.file_size:>9}  {i.filename}")
print(out)
print("sha256", hashlib.sha256(open(out, "rb").read()).hexdigest())
