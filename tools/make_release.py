"""Builds dist/LoS2Control.<version>.zip: the FOMOD installer of LoS2 Control.

    python tools/make_release.py <version> <new DLL> <older DLL> <previous release zip> [--languages]

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
with_languages = "--languages" in sys.argv[5:]  # community translations ship only when asked for
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

# Addon files kept in the repository (addon/) win over the ones in the previous release zip: they are the co-author's current files.
_ad = os.path.join(root, "addon")
if os.path.isdir(_ad):
    for _d, _, _fs in os.walk(_ad):
        for _f in _fs:
            _p = os.path.join(_d, _f)
            files["addon/" + os.path.relpath(_p, _ad).replace(os.sep, "/")] = open(_p, "rb").read()

for name in ("LICENSE", "NOTICE", "README.md"):
    files[name] = rd(name)
files["core/Interface/LoS2Control/los2_logo.png"] = rd("package/Interface/LoS2Control/los2_logo.png")
files["core/SKSE/Plugins/LoS2Control/lang/_template.json"] = rd("package/SKSE/Plugins/LoS2Control/lang/_template.json")
files["core/SKSE/Plugins/LoS2Control_Weathers.txt"] = rd("package/SKSE/Plugins/LoS2Control_Weathers.txt")
files["dll-ng/SKSE/Plugins/LoS2Control.dll"] = open(dll_new, "rb").read()
files["dll-legacy/SKSE/Plugins/LoS2Control.dll"] = open(dll_old, "rb").read()

# Community translations (only with --languages): lang-src/community/<language>.json, each becomes an optional "Menu language" choice in the installer.
# (Only a file in that folder ships; a draft in package/ never does.)
comm = os.path.join(root, "lang-src", "community")
langs = sorted(f[:-5] for f in os.listdir(comm) if f.endswith(".json")) if (with_languages and os.path.isdir(comm)) else []
for l in langs:
    files[f"lang-{l}/SKSE/Plugins/LoS2Control/lang/{l}.json"] = rd(f"lang-src/community/{l}.json")
if langs:
    cfg = files["fomod/ModuleConfig.xml"].decode("utf-8")
    assert "Menu language" not in cfg
    plugins = "".join(
        f'''
        <plugin name="{l.capitalize()}">
          <description>Menu translation ({l}). The menu follows the language the game runs in. Needs the {l} font switched on in SKSEMenuFramework.ini, under [Fonts].</description>
          <files><folder source="lang-{l}" destination="" priority="0"/></files>
          <typeDescriptor><type name="Optional"/></typeDescriptor>
        </plugin>''' for l in langs)
    step = f'''    <installStep name="Menu language">
      <optionalFileGroups order="Explicit">
        <group name="Menu language (optional)" type="SelectAny">
          <plugins order="Explicit">{plugins}
          </plugins>
        </group>
      </optionalFileGroups>
    </installStep>
  '''
    assert cfg.count("</installSteps>") == 1
    cfg = cfg.replace("</installSteps>", step + "</installSteps>")
    xml.dom.minidom.parseString(cfg)
    files["fomod/ModuleConfig.xml"] = cfg.encode("utf-8")
assert not any("japanese" in n.lower() and not n.startswith("lang-") for n in files), "a translation draft must not ship"

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
