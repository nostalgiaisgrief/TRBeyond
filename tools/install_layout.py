"""Move pre-multi-game installations without replacing user data."""
from pathlib import Path
import shutil
from contextlib import contextmanager


def create_game_directories(root):
    for game in range(1, 6):
        for folder in ('Source', 'Config', 'Custom'):
            (Path(root)/folder/f'TR{game}').mkdir(parents=True, exist_ok=True)


def migrate(root):
    root = Path(root).resolve()
    moves = [(root/'Source'/edition, root/'Source/TR1'/edition)
             for edition in ('DOS', 'PS1', 'TRR', 'UB')]
    custom = root/'Custom'
    if custom.is_dir():
        moves += [(p, custom/'TR1'/p.name) for p in custom.iterdir()
                  if p.name[:1].isdigit()]
    moves = [(old, new) for old, new in moves if old.exists() or old.is_symlink()]
    for old, new in moves:
        if new.exists() or new.is_symlink():
            raise ValueError(f'Both old and new game folders exist: {old} and {new}. No files were replaced.')
    create_game_directories(root)
    for old, new in moves:
        new.parent.mkdir(parents=True, exist_ok=True)
        old.rename(new)
    settings = root/'Config/TR1/settings.ini'
    settings.parent.mkdir(parents=True, exist_ok=True)
    if not settings.exists() and (root/'settings.ini').is_file():
        shutil.copy2(root/'settings.ini', settings)
    (root/'Custom/TR1').mkdir(parents=True, exist_ok=True)


@contextmanager
def migration_transaction(root):
    root = Path(root).resolve()
    originals = [root/'Source'/e for e in ('DOS', 'PS1', 'TRR', 'UB')]
    custom = root/'Custom'
    if custom.is_dir():
        originals += [p for p in custom.iterdir() if p.name[:1].isdigit()]
    pairs = [(p, p.parent/'TR1'/p.name) for p in originals if p.exists() or p.is_symlink()]
    config = root/'Config/TR1/settings.ini'
    had_config = config.exists()
    try:
        migrate(root)
        yield
    except Exception:
        for old, new in reversed(pairs):
            if not old.exists() and not old.is_symlink() and (new.exists() or new.is_symlink()):
                new.rename(old)
        if not had_config and config.is_file():
            config.unlink()
        raise
