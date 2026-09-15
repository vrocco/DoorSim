#!/usr/bin/env python3
import json
import re
from copy import deepcopy
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DATA_PATH = ROOT / "data" / "wiegand_formats.json"
MAIN_PATH = ROOT / "src" / "main.cpp"


def load_data_catalog():
    return json.loads(DATA_PATH.read_text())["wiegandFormats"]


def load_embedded_catalog():
    text = MAIN_PATH.read_text()
    match = re.search(
        r'const char defaultWiegandFormatsJson\[\] = R"json\((.*?)\)json";',
        text,
        re.S,
    )
    assert match, "embedded defaultWiegandFormatsJson not found"
    return json.loads(match.group(1))["wiegandFormats"]


def bits_from_int(value, width):
    return [(value >> shift) & 1 for shift in range(width - 1, -1, -1)]


def set_field(bits, start, end, value):
    width = end - start + 1
    encoded = bits_from_int(value, width)
    for offset, bit in enumerate(encoded):
        bits[start - 1 + offset] = bit


def apply_rule(bits, rule):
    ones = sum(bits[pos - 1] for pos in rule["bits"])
    want_even = rule["type"] == "even"
    # The target bit participates in the total parity calculation.
    bits[rule["bit"] - 1] = ones % 2 if want_even else (ones + 1) % 2


def build_s12906(fc=12, issue=1, cn=123456):
    bits = [0] * 36
    set_field(bits, 2, 9, fc)
    set_field(bits, 10, 11, issue)
    set_field(bits, 12, 35, cn)
    for rule in s12906_format()["parityRules"]:
        apply_rule(bits, rule)
    return bits


def build_c15001(oem=900, fc=12, cn=12345):
    bits = [0] * 36
    set_field(bits, 2, 11, oem)
    set_field(bits, 12, 19, fc)
    set_field(bits, 20, 35, cn)
    for rule in c15001_format()["parityRules"]:
        apply_rule(bits, rule)
    return bits


def has_parity(fmt):
    return bool(fmt.get("parityRules")) or (
        fmt.get("parityEvenBit", 0) > 0 and fmt.get("parityOddBit", 0) > 0
    )


def parity_ok(fmt, bits):
    if len(bits) != fmt["bitCount"]:
        return False
    rules = fmt.get("parityRules") or []
    if rules:
        for rule in rules:
            ones = bits[rule["bit"] - 1]
            ones += sum(bits[pos - 1] for pos in rule["bits"])
            if rule["type"] == "even":
                if ones % 2 != 0:
                    return False
            else:
                if ones % 2 != 1:
                    return False
        return True

    required = [
        "parityEvenBit",
        "parityEvenStart",
        "parityEvenEnd",
        "parityOddBit",
        "parityOddStart",
        "parityOddEnd",
    ]
    if not all(fmt.get(field, 0) > 0 for field in required):
        return False
    even = bits[fmt["parityEvenBit"] - 1]
    even += sum(bits[pos - 1] for pos in range(fmt["parityEvenStart"], fmt["parityEvenEnd"] + 1))
    odd = bits[fmt["parityOddBit"] - 1]
    odd += sum(bits[pos - 1] for pos in range(fmt["parityOddStart"], fmt["parityOddEnd"] + 1))
    return even % 2 == 0 and odd % 2 == 1


def select_format(catalog, bits):
    matches = [fmt for fmt in catalog if fmt["bitCount"] == len(bits)]
    if len(matches) == 1:
        return matches[0], len(matches), 1
    viable = [fmt for fmt in matches if not has_parity(fmt) or parity_ok(fmt, bits)]
    return (viable[0] if len(viable) == 1 else None), len(matches), len(viable)


def s12906_format():
    return next(fmt for fmt in load_data_catalog() if fmt["id"] == "hid-s12906-36")


def c15001_format():
    return next(fmt for fmt in load_data_catalog() if fmt["id"] == "hid-c15001-36")


def migrate_defaults(existing, defaults):
    by_id = {fmt.get("id", ""): fmt for fmt in existing if fmt.get("id")}
    descriptions = {fmt.get("description", "") for fmt in existing}
    changed = False
    for default in defaults:
        default_id = default.get("id", "")
        if default_id in by_id or default.get("description", "") in descriptions:
            continue
        existing.append(deepcopy(default))
        changed = True
    return existing, changed


def test_catalog_ids_are_unique():
    ids = [fmt.get("id") for fmt in load_data_catalog()]
    assert all(ids), "every built-in format must have a stable id"
    assert len(ids) == len(set(ids)), "built-in format ids must be unique"


def test_s12906_definition():
    fmt = s12906_format()
    assert fmt["description"] == "HID S12906 36-bit"
    assert (fmt["facilityCodeStart"], fmt["facilityCodeEnd"]) == (2, 9)
    assert (fmt["cardNumberStart"], fmt["cardNumberEnd"]) == (12, 35)
    assert fmt["parityRules"] == [
        {"bit": 1, "type": "odd", "bits": list(range(2, 19))},
        {"bit": 36, "type": "odd", "bits": list(range(18, 36))},
    ]


def test_c15001_definition():
    fmt = c15001_format()
    assert fmt["description"] == "HID KeyScan 36-bit"
    assert (fmt["facilityCodeStart"], fmt["facilityCodeEnd"]) == (12, 19)
    assert (fmt["cardNumberStart"], fmt["cardNumberEnd"]) == (20, 35)
    assert fmt["parityRules"] == [
        {"bit": 1, "type": "even", "bits": list(range(2, 19))},
        {"bit": 36, "type": "odd", "bits": list(range(19, 36))},
    ]


def test_embedded_defaults_include_new_formats_for_migration():
    embedded_ids = {fmt.get("id") for fmt in load_embedded_catalog()}
    assert "hid-s12906-36" in embedded_ids
    assert "hid-c15001-36" in embedded_ids


def test_migration_adds_new_formats_by_stable_id():
    existing = [
        fmt for fmt in load_data_catalog()
        if fmt.get("id") not in {"hid-s12906-36", "hid-c15001-36"}
    ]
    migrated, changed = migrate_defaults(existing, load_embedded_catalog())
    assert changed
    assert sum(1 for fmt in migrated if fmt.get("id") == "hid-s12906-36") == 1
    assert sum(1 for fmt in migrated if fmt.get("id") == "hid-c15001-36") == 1


def test_unique_candidate_decodes_even_with_bad_parity():
    bits = build_s12906()
    bits[0] ^= 1
    selected, candidates, viable = select_format([s12906_format()], bits)
    assert candidates == 1
    assert viable == 1
    assert selected is not None
    assert selected["id"] == "hid-s12906-36"


def test_multiple_candidates_can_select_one_parity_viable():
    good = s12906_format()
    other = deepcopy(good)
    other["id"] = "test-conflicting-36"
    other["parityRules"] = [{"bit": 1, "type": "even", "bits": list(range(2, 19))}]
    selected, candidates, viable = select_format([good, other], build_s12906())
    assert candidates == 2
    assert viable == 1
    assert selected is not None
    assert selected["id"] == "hid-s12906-36"


def test_s12906_and_c15001_parity_can_discriminate():
    selected, candidates, viable = select_format(
        [s12906_format(), c15001_format()],
        build_c15001(),
    )
    assert candidates == 2
    assert viable == 1
    assert selected is not None
    assert selected["id"] == "hid-c15001-36"


def test_multiple_viable_candidates_are_ambiguous():
    good = s12906_format()
    same = deepcopy(good)
    same["id"] = "test-same-parity-36"
    selected, candidates, viable = select_format([good, same], build_s12906())
    assert candidates == 2
    assert viable == 2
    assert selected is None


def test_parityless_candidate_keeps_real_s12906_ambiguous():
    catalog = [fmt for fmt in load_data_catalog() if fmt.get("bitCount") == 36]
    selected, candidates, viable = select_format(catalog, build_s12906())
    assert {fmt["id"] for fmt in catalog} >= {"doorsim-36-legacy", "hid-s12906-36"}
    assert candidates >= 2
    assert viable >= 2
    assert selected is None


def test_zero_viable_when_all_candidates_have_bad_parity():
    bits = build_s12906()
    bits[0] ^= 1
    # Single-candidate legacy behavior decodes despite bad parity; add a second
    # parity-aware format to exercise the multi-candidate zero-viable path.
    other = deepcopy(s12906_format())
    other["id"] = "test-other-36"
    selected, candidates, viable = select_format([s12906_format(), other], bits)
    assert candidates == 2
    assert viable == 0
    assert selected is None


def main():
    tests = [value for name, value in sorted(globals().items()) if name.startswith("test_")]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")
    print(f"{len(tests)} tests passed")


if __name__ == "__main__":
    main()
