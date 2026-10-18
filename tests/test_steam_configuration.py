"""Steam migration regression: preserve unrelated app/account settings."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('steam_config',Path(__file__).resolve().parents[1]/'tools/configure_steam_remoteplay.py')
c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)

def vdf(apps):
    return '"UserLocalConfigStore" { "Software" { "Valve" { "Steam" { "PersonaState" "1" "apps" { '+apps+' } } } } }'

def values(text):
    def convert(entries):return {n['key']:convert(n['children']) if 'children' in n else n['value'] for n in entries}
    return convert(c.nodes(text))

class ConfigurationTests(unittest.TestCase):
    def test_mixed_case_real_config_preservation_and_mutation(self):
        before=vdf('"480" { "Playtime" "7" }').replace('"Steam"','"steam"')
        after=c.replace(before,'our wrapper')
        c.check_preserved_settings(before,after)
        with self.assertRaises(RuntimeError):
            c.check_preserved_settings(before,after.replace('"Playtime" "7"','"Playtime" "8"'))

    def test_new_spacewar_preserves_other_games(self):
        before=vdf('"858210" { "LaunchOptions" "-native-option" "Playtime" "42" } "1054430" { "LaunchOptions" "-user-option" }')
        desired='"C:\\Games\\Nova Drift Coop\\SteamCoop.exe" --host %command%'
        after=c.replace(before,desired)
        self.assertEqual(c.option(after)['value'],desired)
        expected=values(before);expected['UserLocalConfigStore']['Software']['Valve']['Steam']['apps']['480']={'LaunchOptions':desired}
        self.assertEqual(values(after),expected)

    def test_restore_preserves_values_added_by_steam(self):
        before=vdf('"480" { "name" "Spacewar" "Playtime" "21" "LaunchOptions" "old" }')
        configured=c.replace(before,'new')
        after_steam=configured.replace('"Playtime" "21"','"Playtime" "28" "LastPlayed" "99"')
        restored=c.replace(after_steam,'old')
        self.assertEqual(c.option(restored)['value'],'old')
        self.assertIn('"Playtime" "28" "LastPlayed" "99"',restored)

    def test_removing_only_option_leaves_other_app_fields(self):
        text=vdf('"480" { "name" "Spacewar" "LaunchOptions" "custom" "Playtime" "7" }')
        result=c.replace(text,None)
        self.assertIsNone(c.option(result))
        self.assertIn('"Playtime" "7"',result)

    def test_fling_migration_does_not_erase_new_user_option(self):
        with tempfile.TemporaryDirectory() as d,patch.object(c,'OUT',Path(d)):
            record={'config':'/owned/localconfig.vdf','launch_option':None,'approved_launch_option':'our old wrapper'}
            (Path(d)/'original-setting.json').write_text(json.dumps(record))
            before=vdf('"1054430" { "LaunchOptions" "new user setting" }')
            after,changed=c.legacy_fling_restore(before,Path(record['config']))
            self.assertFalse(changed);self.assertEqual(after,before)

    def test_fling_migration_restores_only_recorded_option(self):
        with tempfile.TemporaryDirectory() as d,patch.object(c,'OUT',Path(d)):
            record={'config':'/owned/localconfig.vdf','launch_option':'-prior-user-setting','approved_launch_option':'our old wrapper'}
            (Path(d)/'original-setting.json').write_text(json.dumps(record))
            before=vdf('"1054430" { "LaunchOptions" "our old wrapper" "Playtime" "90" }')
            after,changed=c.legacy_fling_restore(c.replace(before,'spacewar wrapper'),Path(record['config']))
            self.assertTrue(changed);self.assertEqual(c.option(after,'1054430')['value'],'-prior-user-setting')
            self.assertEqual(c.option(after)['value'],'spacewar wrapper');self.assertIn('"Playtime" "90"',after)

    def test_wrong_profile_cannot_trigger_legacy_restore(self):
        with tempfile.TemporaryDirectory() as d,patch.object(c,'OUT',Path(d)):
            (Path(d)/'original-setting.json').write_text(json.dumps({'config':'/other/profile','launch_option':None,'approved_launch_option':'ours'}))
            before=vdf('"1054430" { "LaunchOptions" "ours" }')
            self.assertEqual(c.legacy_fling_restore(before,Path('/current/profile')),(before,False))

    def test_duplicate_app_and_option_are_rejected(self):
        for text in (vdf('"480" {} "480" {}'),vdf('"480" { "LaunchOptions" "a" "launchoptions" "b" }')):
            with self.assertRaises(ValueError):c.option(text)

    def test_crlf_and_quoted_paths_round_trip(self):
        before=vdf('"480" {\r\n "name" "Spacewar"\r\n }')
        desired='"C:\\Users\\Example\\Games\\Nova Drift\\SteamCoop.exe" --host %command%'
        after=c.replace(before,desired)
        self.assertEqual(c.option(after)['value'],desired);self.assertIn('\r\n',after)

if __name__=='__main__':unittest.main()
