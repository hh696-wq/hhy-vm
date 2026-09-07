#!/usr/bin/env python3
"""Minimal install.hhy database lifecycle fixture; not a complete CMS product."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

INSTALL = '''import database
let cfg = read_text(path(args[0])) |> parse_json
let mode = args[1]
database.execute(cfg, "CREATE TABLE IF NOT EXISTS hhy_cms_version (id INT PRIMARY KEY, version INT)", [])
database.execute(cfg, "CREATE TABLE IF NOT EXISTS hhy_cms_articles (id INT PRIMARY KEY, title VARCHAR(200))", [])
let status = attempt {
    cfg |> database.with_transaction { tx ->
        let installed = database.query(tx, "SELECT version FROM hhy_cms_version WHERE id = 1", [])
        if length(installed.rows) == 0 {
            database.execute(tx, "INSERT INTO hhy_cms_version VALUES (1, 1)", [])
            database.execute(tx, "INSERT INTO hhy_cms_articles VALUES (1, 'Hello CMS')", [])
        }
        if mode == "interrupt" {
            database.execute(tx, "INSERT INTO hhy_cms_version VALUES (1, 99)", [])
        }
        if mode == "upgrade-fail" {
            database.execute(tx, "UPDATE hhy_cms_version SET version = 2 WHERE id = 1", [])
            database.execute(tx, "INSERT INTO hhy_cms_articles VALUES (1, 'duplicate')", [])
        }
    }
}
print(status.ok)
'''
BACKUP = '''import database
let cfg = read_text(path(args[0])) |> parse_json
let rows = database.query(cfg, "SELECT id, title FROM hhy_cms_articles ORDER BY id", []).rows
write_text(path(args[1]), encode_json(rows))
database.execute(cfg, "DELETE FROM hhy_cms_articles", [])
let saved = read_text(path(args[1])) |> parse_json
cfg |> database.with_transaction { tx ->
    for row in saved { database.execute(tx, args[2], [to_int(row.id), row.title]) }
}
database.query(cfg, "SELECT id, title FROM hhy_cms_articles ORDER BY id", []).rows |> encode_json |> print
'''

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--config', type=Path, required=True)
    args = ap.parse_args()
    hhy = str(Path('build/hhy').resolve())
    with tempfile.TemporaryDirectory(prefix='hhy-cms-db-') as temp:
        temp = Path(temp)
        env = dict(os.environ, HHY_EXTENSION_HOME=str(temp/'extensions'))
        subprocess.run([hhy, 'install', '--yes', 'extensions/database'], env=env,
                       check=True, stdout=subprocess.DEVNULL)
        install = temp/'install.hhy'; install.write_text(INSTALL)
        backup = temp/'backup.hhy'; backup.write_text(BACKUP)
        from integration import Client
        for cfg in json.loads(args.config.read_text()):
            cp = temp/'config.json'; cp.write_text(json.dumps(cfg)); cp.chmod(0o600)
            c = Client(Path('extensions/database/bin/hhy-database').resolve())
            try:
                def reset():
                    for table in ['hhy_cms_articles', 'hhy_cms_version']:
                        c.call('execute', cfg, 'DROP TABLE IF EXISTS '+table, [])
                for engine in ['ast', 'bytecode']:
                    reset()
                    for mode, ok in [('interrupt', False), ('install', True),
                                     ('install', True), ('upgrade-fail', False)]:
                        result = subprocess.run([hhy, 'run', '--engine', engine, str(install),
                                                 str(cp), mode], env=env, check=False,
                                                capture_output=True, text=True, timeout=30)
                        assert result.returncode == 0, result.stderr
                        assert result.stdout.strip() == str(ok).lower(), result
                        rows = c.call('query', cfg, 'SELECT version FROM hhy_cms_version', [])['rows']
                        assert rows == ([] if mode == 'interrupt' else [{'version': '1'}]), rows
                    sql = ('INSERT INTO hhy_cms_articles VALUES (?, ?)' if cfg['driver']=='mysql'
                           else 'INSERT INTO hhy_cms_articles VALUES ($1, $2)')
                    result = subprocess.run([hhy, 'run', '--engine', engine, str(backup), str(cp),
                                             str(temp/'backup.json'), sql], env=env, check=False,
                                            capture_output=True, text=True, timeout=30)
                    assert result.returncode == 0, result.stderr
                    assert json.loads(result.stdout) == [{'id':'1', 'title':'Hello CMS'}]
                reset()
                print(f"{cfg['driver']}: install interruption, repeat install, failed upgrade and backup restore passed")
            finally:
                c.close()

if __name__ == '__main__':
    main()
