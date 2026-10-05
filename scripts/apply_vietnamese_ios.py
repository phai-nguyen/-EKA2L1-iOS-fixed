#!/usr/bin/env python3
from pathlib import Path
import json
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: apply_vietnamese_ios.py <eka2l1-source>")

source = Path(sys.argv[1]).resolve()
control = Path(__file__).resolve().parents[1]

target_loc = source / "src/emu/ios/Resources/Localizable.xcstrings"
target_info = source / "src/emu/ios/Resources/InfoPlist.xcstrings"
reference_loc = control / "localization/Localizable.vi-reference.xcstrings"
reference_info = control / "localization/InfoPlist.vi-reference.xcstrings"

def load(path):
    return json.loads(path.read_text(encoding="utf-8"))

def save(path, obj):
    path.write_text(json.dumps(obj, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

def merge_vi(target, reference):
    for key, ref_entry in reference.get("strings", {}).items():
        if key not in target.get("strings", {}):
            continue
        vi = ref_entry.get("localizations", {}).get("vi")
        if vi is not None:
            target["strings"][key].setdefault("localizations", {})["vi"] = vi

loc = load(target_loc)
info = load(target_info)
ref_loc = load(reference_loc)
ref_info = load(reference_info)

merge_vi(loc, ref_loc)
merge_vi(info, ref_info)

# New upstream strings added after the device-tested Vietnamese 26.9.2 catalog.
direct = {
    "devices.delete.message": "Dữ liệu của thiết bị này cũng sẽ bị xoá cùng thiết bị.",
    "devices.delete.title": "Xoá thiết bị?",
    "import.isolateDrives": "Bộ nhớ riêng cho thiết bị này",
    "import.isolateDrives.footer": "Các ổ C, D và E của thiết bị này được tách riêng khỏi các thiết bị khác.",
    "import.source.vpl": "Firmware VPL",
    "import.vpl.chooseFolder": "Chọn thư mục firmware",
    "import.vpl.chooseFiles": "Chọn các tệp firmware",
    "import.vpl.hint": "Chọn thư mục chứa tệp khai báo .vpl và toàn bộ tệp firmware tương ứng, hoặc chọn các tệp đó cùng lúc.",
    "import.vpl.noManifest": "Chưa chọn tệp khai báo .vpl.",
}
for key, value in direct.items():
    if key in loc["strings"]:
        loc["strings"][key].setdefault("localizations", {})["vi"] = {
            "stringUnit": {"state": "translated", "value": value}
        }

# HYBRIDHOME3 host-shell strings are introduced by the build-time patcher.
# Create both English and Vietnamese localizations so the shell follows the
# app language instead of hard-coding Vietnamese UI text.
hybrid_strings = {
    "hybridhome.shortcuts": ("Shortcuts", "Lối tắt"),
    "hybridhome.openMenu": ("Open application menu", "Mở Menu ứng dụng"),
    "hybridhome.backendHint": (
        "Applications and icons come from the firmware AppList/AppArc registry.",
        "Ứng dụng và biểu tượng được lấy từ AppList/AppArc của firmware."
    ),
    "hybridhome.menuTitle": ("Applications", "Menu ứng dụng"),
    "hybridhome.home": ("Home", "Trang chủ"),
    "hybridhome.menu": ("Menu", "Menu"),
    "hybridhome.phone": ("Phone", "Điện thoại"),
    "hybridhome.contacts": ("Contacts", "Danh bạ"),
    "hybridhome.search": ("Search applications or UID", "Tìm ứng dụng hoặc UID"),
    "hybridhome.noResults": ("No matching applications", "Không tìm thấy ứng dụng phù hợp"),
    "hybridhome.refresh": ("Refresh applications", "Làm mới ứng dụng"),
}
for key, (en_value, vi_value) in hybrid_strings.items():
    entry = loc["strings"].setdefault(key, {"localizations": {}})
    entry.setdefault("localizations", {})["en"] = {
        "stringUnit": {"state": "translated", "value": en_value}
    }
    entry["localizations"]["vi"] = {
        "stringUnit": {"state": "translated", "value": vi_value}
    }

# Vietnamese uses a single plural form here, so "other" is sufficient.
plural = {
    "home.banner.installedPackages %lld": "Đã cài đặt %lld gói.",
    "home.banner.installingPackages %lld": "Đang cài đặt %lld gói...",
    "home.fonts.imported %lld": "Đã cài đặt %lld phông chữ. Khởi động lại thiết bị để sử dụng.",
    "home.fonts.importing %lld": "Đang cài đặt %lld phông chữ…",
    "home.ngage2.imported %lld": "Đã nhập %lld gói. Mở trình khởi chạy N-Gage để tiếp tục.",
    "home.ngage2.importing %lld": "Đang nhập %lld gói N-Gage 2.0...",
    "import.vpl.fileCount %lld": "Đã chọn %lld tệp firmware.",
    "hybridhome.appCount %lld": "%lld ứng dụng Symbian",
}
if "hybridhome.appCount %lld" not in loc["strings"]:
    loc["strings"]["hybridhome.appCount %lld"] = {
        "localizations": {
            "en": {
                "variations": {
                    "plural": {
                        "other": {
                            "stringUnit": {
                                "state": "translated",
                                "value": "%lld Symbian applications"
                            }
                        }
                    }
                }
            }
        }
    }

for key, value in plural.items():
    if key in loc["strings"]:
        loc["strings"][key].setdefault("localizations", {})["vi"] = {
            "variations": {
                "plural": {
                    "other": {
                        "stringUnit": {"state": "translated", "value": value}
                    }
                }
            }
        }

if "NSLocationWhenInUseUsageDescription" in info["strings"]:
    info["strings"]["NSLocationWhenInUseUsageDescription"].setdefault("localizations", {})["vi"] = {
        "stringUnit": {
            "state": "translated",
            "value": "Các ứng dụng Symbian chạy trong trình giả lập có thể dùng vị trí của bạn cho bản đồ và các tính năng định vị."
        }
    }

def missing_vi(catalog):
    missing = []
    for key, entry in catalog.get("strings", {}).items():
        if "vi" not in entry.get("localizations", {}):
            missing.append(key)
    return missing

missing_loc = missing_vi(loc)
missing_info = missing_vi(info)
if missing_loc or missing_info:
    raise SystemExit(
        "Vietnamese localization incomplete:\n"
        + "Localizable missing: " + repr(missing_loc) + "\n"
        + "InfoPlist missing: " + repr(missing_info)
    )

save(target_loc, loc)
save(target_info, info)

print(f"[VI-LOCALIZATION] Localizable: {len(loc['strings'])}/{len(loc['strings'])}")
print(f"[VI-LOCALIZATION] InfoPlist: {len(info['strings'])}/{len(info['strings'])}")
print("[VI-LOCALIZATION] COMPLETE")
