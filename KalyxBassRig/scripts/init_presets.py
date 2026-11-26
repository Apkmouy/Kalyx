#!/usr/bin/env python3
"""
Generates the 5 signature factory preset XML files in Resources/Presets/
calibrated to 0 VU = -18 dBFS for Kalyx Bass Rig v1.0.0.
"""

import os
import xml.etree.ElementTree as ET

PRESETS = {
    "01_Indie_Pick_Classic_Tube_Chorus": {
        "amp_model": 1, "amp_gain": 46.0, "amp_bass": 1.5, "amp_mid": 2.0,
        "amp_mid_freq": 2, "amp_treble": 3.0, "amp_drive": 24.0, "cab_a_ir": 0,
        "fx_tdrv_en": 1, "fx_tdrv_drv": 28.0, "fx_tdrv_blend": 65.0,
        "fx_cho_en": 1, "fx_cho_rate": 0.85, "fx_cho_depth": 48.0, "fx_cho_mix": 38.0,
        "di_circuit": 1, "di_level": -7.0, "amp_level": -1.0, "out_fader": 0.0, "vu_ref": -18.0,
    },
    "02_Modern_Metal_4PRO_B3K": {
        "amp_model": 2, "gate_en": 1, "gate_thresh": -44.0, "amp_gain": 52.0,
        "amp_bright": 1, "amp_ultra_lo": 4, "amp_ultra_hi": 1, "eq9_en": 1,
        "eq9_40": 3.0, "eq9_90": 2.5, "eq9_500": -6.0, "eq9_2k": 4.5, "eq9_4k": 3.5,
        "fx_tdrv_en": 1, "fx_tdrv_drv": 78.0, "fx_tdrv_tone": 68.0, "fx_tdrv_blend": 42.0,
        "cab_a_ir": 8, "di_circuit": 2, "di_level": -4.5, "amp_level": -1.5, "out_fader": -1.0, "vu_ref": -18.0,
    },
    "03_Motown_Thump_Heritage_Flatwound": {
        "amp_model": 0, "amp_channel": 1, "amp_gain": 48.0, "amp_ultra_lo": 3,
        "amp_ultra_hi": 0, "amp_bass": 6.5, "amp_mid": 1.5, "amp_treble": -8.0,
        "mic_type": 5, "mic_pos_y": 25.0, "fx_comp_en": 1, "fx_comp_mode": 0,
        "fx_comp_amt": 55.0, "fx_comp_blend": 80.0, "di_circuit": 1,
        "di_lpf_freq": 420.0, "di_level": -4.0, "amp_level": -1.0, "out_fader": 0.0, "vu_ref": -18.0,
    },
    "04_Doom_Wall_Tangerine_Fuzz": {
        "amp_model": 3, "amp_gain": 82.0, "amp_depth": 0, "amp_boost": 80.0,
        "amp_bass": 5.0, "amp_mid": 2.5, "amp_treble": 1.5, "amp_drive": 85.0, "amp_sag": 65.0,
        "fx_fuzz_en": 1, "fx_fuzz_sust": 100.0, "fx_fuzz_tilt": -15.0, "fx_fuzz_low": 65.0,
        "cab_a_ir": 0, "di_circuit": 3, "di_level": -10.0, "amp_level": -2.5, "out_fader": -2.0, "vu_ref": -18.0,
    },
    "05_Synth_Dub_SubOctave_Tape": {
        "amp_en": 0, "cab_en": 0, "di_circuit": 0, "di_lpf_freq": 200.0,
        "di_level": 0.0, "amp_level": -60.0, "fx_oct_en": 1, "fx_oct_clean": 45.0,
        "fx_oct_sub1": 100.0, "fx_oct_sub2": 100.0, "fx_oct_up": 0.0,
        "fx_env_en": 1, "fx_env_sens": 65.0, "fx_env_q": 4.5,
        "fx_tape_en": 1, "fx_tape_sync": 1, "fx_tape_div": 2, "fx_tape_rep": 48.0, "fx_tape_mix": 34.0,
        "out_fader": -1.5, "vu_ref": -18.0,
    },
}


def main() -> None:
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_dir = os.path.join(root, "Resources", "Presets")
    os.makedirs(out_dir, exist_ok=True)
    for name, params in PRESETS.items():
        root_el = ET.Element("KALYX_BASS_RIG_STATE")
        for pid, val in params.items():
            ET.SubElement(root_el, "PARAM", id=pid, value=str(val))
        tree = ET.ElementTree(root_el)
        ET.indent(tree, space="  ")
        path = os.path.join(out_dir, f"{name}.xml")
        tree.write(path, encoding="utf-8", xml_declaration=True)
        print(f"[init_presets] Wrote {path}")


if __name__ == "__main__":
    main()
