"""Disc-free release regression suite, suitable for a clean checkout and CI."""
from pathlib import Path
import subprocess
import sys
import fetch_tools

ROOT = Path(__file__).resolve().parents[2]
TESTS = ('test_partner_recipes.py', 'test_tag_deck_sandbox.py',
         'test_tag_opening_ai.py', 'test_tag_rank.py',
         'test_wide_lp_hud.py', 'test_texture_clear.py', 'test_duel_modules.py',
         'test_duel_match_runtime.py', 'test_duel_preferences.py', 'test_tag_rewards.py',
         'test_new_card_markers.py', 'test_duel_visual_lifecycle.py',
         'test_deck_menu_flows.py', 'test_tag_input_routing.py', 'test_release_polish.py')

def main():
    fetch_tools.ensure('llvm-mingw')
    for test in TESTS:
        print('Checking', test, flush=True)
        subprocess.run([sys.executable, str(ROOT/'tools/pc'/test)], cwd=ROOT, check=True)
    print('All disc-free release checks passed. Full gameplay/platform acceptance remains separate.')

if __name__ == '__main__':
    main()
