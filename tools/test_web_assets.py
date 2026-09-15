#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def text(path):
    return (ROOT / path).read_text()


def test_dashboard_and_sections_exist():
    html = text("data/index.html")
    for required in [
        'id="currentCard"',
        'id="lastReadCardsTable"',
        'id="formatTable"',
        'id="rawDecodeInput"',
        'id="statusPanel"',
        "showSection('formats')",
        "showSection('tools')",
        "showSection('status')",
    ]:
        assert required in html


def test_ctf_authorize_from_last_read_controls_exist():
    js = text("data/script.js")
    for required in [
        "function canAuthorize(card)",
        "function authorizeCardFromRead(index)",
        "formatCandidateCount === 1",
        "formatViableCount === 1",
        "new URLSearchParams",
    ]:
        assert required in js


def test_format_catalog_endpoint_wired():
    js = text("data/script.js")
    cpp = text("src/main.cpp")
    assert "fetch('/getWiegandFormats')" in js
    assert 'server.on("/getWiegandFormats"' in cpp
    for field in [
        'item["id"]',
        'item["description"]',
        'item["bitCount"]',
        'item["hasParity"]',
    ]:
        assert field in cpp


def test_add_card_duplicate_guard_present():
    cpp = text("src/main.cpp")
    assert "Credential already exists" in cpp
    assert "checkCredential(facilityCode, cardNumber)" in cpp


def test_raw_decode_and_status_endpoints_wired():
    js = text("data/script.js")
    cpp = text("src/main.cpp")
    ini = text("platformio.ini")
    for required in [
        "fetch(`/decodeRaw?bits=",
        "function renderCandidateRows(candidates)",
        "function exportScanHistory()",
        "fetch('/exportScans')",
        "fetch('/getStatus')",
    ]:
        assert required in js
    for required in [
        'server.on("/decodeRaw"',
        'server.on("/exportScans"',
        'server.on("/getStatus"',
        'doc["diagnosticOnly"] = true',
        'doc["eligibleForAuthorization"] = false',
        'DOORSIM_BUILD_COMMIT',
    ]:
        assert required in cpp
    assert "extra_scripts = pre:tools/build_version.py" in ini


def test_styles_cover_diagnostics():
    css = text("data/style.css")
    for required in [".card-panel", ".diagnostic", ".authorized", ".unauthorized"]:
        assert required in css


def main():
    tests = [value for name, value in sorted(globals().items()) if name.startswith("test_")]
    for test in tests:
        test()
        print(f"PASS {test.__name__}")
    print(f"{len(tests)} tests passed")


if __name__ == "__main__":
    main()
