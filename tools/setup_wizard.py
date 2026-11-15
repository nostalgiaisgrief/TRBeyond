"""Portable Tomb Raider Beyond setup wizard; freeze with build-setup.ps1."""
from pathlib import Path
from typing import NamedTuple
import os
import queue
import re
import subprocess
import sys
import tempfile
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from setup_import import install, install_sources, dos_source_kind, inspect_source, inspect_installation
from setup_import import SPACE_PER_SOURCE_GIB


VERSION_PATH = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parent.parent)) / 'version.txt'
VERSION = VERSION_PATH.read_text(encoding='utf-8-sig').strip()
if not re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', VERSION) or any(int(v) > 65535 for v in VERSION.split('.')):
    raise ValueError('Invalid version.txt: expected major.minor.patch, each between 0 and 65535.')


class Setting(NamedTuple):
    section: str
    key: str
    default: str
    options: tuple


TR1_SETTINGS = {
    "Resolution": Setting("Tomb", "detail", "2", (
        ("2", "Full", "Recommended - Uses your desktop resolution for the sharpest image"),
        ("0", "DOS Low", "320 x 200"),
        ("1", "DOS High", "640 x 480"),
        ("3", "PS1", "384 x 240"),
    )),
    "Force 4:3": Setting("Display", "force43", "0", (
        ("1", "On", "Keeps the image in 4:3 for an authentic look. Adds black bars on the sides"),
        ("0", "Off", "Recommended - Uses your desktop or window size. Increases image on the sides for a wider field of view."),
    )),
    "Framerate Limit": Setting("Display", "framerate", "0", (
        ("0", "Off", "Recommended - Uses your desktop framerate. The smoothest experience depending on your rig."),
        ("1", "60", "Locks framerate to 60 frames per second."),
        ("2", "30", "Locks framerate to 30 frames per second, replicating a more authentic experience."),
    )),
    "Colour Depth": Setting("Display", "colour_depth", "Full Colour", (
        ("Full Colour", "Full Colour", "Recommended - Uses the full colour range."),
        ("256 colours", "256 Colours", "Limits to 256 Colours. Generally used for the classic DOS software look. It gives the game a unique visual style with banded shadows, and for some it can look better and moodier in some ways."),
    )),
    "Texture Filtering": Setting("Display", "texture_filtering", "0", (
        ("1", "On", "Makes the textures look smoother. Common in other TR1 ports and the Glide version."),
        ("0", "Off", "Recommended - Keeps textures in their pixelated form."),
    )),
    "Draw Distance": Setting("Display", "draw_distance", "Max Distance with Smooth Fog", (
        ("Original", "Original", "Keeps the original draw distance and fog for an authentic experience."),
        ("Max Distance", "Max Distance", "Removes the draw distance and fog, allowing you to see across entire levels. While nice, this setting can expose odd patches of bright and dark areas in a level all at once. For example, an area far away may be bright, while the area you're in is dark."),
        ("Max Distance with Fog", "Max Distance with Fog", "The best of both worlds above. Fog is present but the rest of the level beyond it can still be very faintly seen. Keeps the moody atmosphere while still being able to judge the size of the area."),
        ("Max Distance with Smooth Fog", "Max Distance with Smooth Fog", "Recommended - Like the previous option, but changes the original fog from Planar to using the actual distance from the camera. In a nutshell, this stops that odd classic Tomb Raider effect where the the fog moves as you look around, which made things on the side of your screen easier to see than things straight in front of you."),
    )),
    "Water Style": Setting("Display", "water_style", "PS1 Colour with DOS Effect", (
        ("DOS", "DOS", "No shading when looking into water. Water looks clear and clean. Nostalgic and better for some tastes."),
        ("DOS 3DFX", "DOS 3DFX", "Shaded a blue-purple colour used in the Glide version of the game."),
        ("PS1", "PS1", "The well-known turquoise colour. Lacks the effect of geometry shifting under the water."),
        ("PS1 Colour with DOS Effect", "PS1 Colour with DOS Effect", "Recommended - The best of both worlds. PS1's turquoise colour with the DOS geometry shifting effect."),
    )),
    "Level Source": Setting("Game", "level_source", "DOS", (
        ("DOS", "DOS", "Recommended - Higher resolution main menu background, better end credit screens, and an extra EIDOS splash screen."),
        ("PS1", "PS1", "Has an extra enemy placement in City of Khamoon... but it is an annoying encounter that you won't miss."),
    )),
    "Music and Speech Source": Setting("Game", "music_speech_source", "PS1+DOS", (
        ("DOS", "DOS", "A single ambient track plays over the whole level. Has a certain moody appeal to it, but you miss a lot of classic music."),
        ("PS1", "PS1", "Extra music that is triggered in key points of the level. Only downside is that most of the time there is no ambient music playing at all."),
        ("PS1+DOS", "DOS+PS1", "Recommended - The best of both worlds. Includes the PS1 music that gets triggered at key points, and then reverts to the DOS background track when no triggered music is playing."),
    )),
    "FMV Source": Setting("Game", "fmv_source", "PS1", (
        ("DOS", "DOS", "Lower quality. Only tiny benefit is a short startup video for EIDOS' Escape video compression toolkit. Which is funny, as this is the worst choice for videos."),
        ("PS1", "PS1", "Recommended - No brainer. The PS1 videos are much better."),
    )),
}


class Wizard:
    def __init__(self, root):
        self.root = root
        root.title(f'Tomb Raider Beyond — Version {VERSION}')
        root.geometry('720x700'); root.minsize(720, 700)
        style = ttk.Style(root)
        style.configure('Heading.TLabel', font=('Segoe UI', 17, 'bold'))
        self.page = 'folder'
        self.tr1_enabled = tk.BooleanVar(value=False)
        self.busy = False
        self.cancel = threading.Event()
        self.events = queue.Queue()
        self.destination = tk.StringVar(value=str(Path.home()/'Games'/'Tomb Raider Beyond'))
        self.kind = tk.StringVar(value='new')
        self.tr1_preset = tk.StringVar(value='Optimal')
        self.custom_values = {title: tk.StringVar(value=setting.default)
                              for title, setting in TR1_SETTINGS.items()}
        self.source_paths = {edition: tk.StringVar() for edition in ('DOS', 'PS1')}
        self.source_enabled = {edition: tk.BooleanVar(value=False) for edition in ('DOS', 'PS1')}
        self.status = tk.StringVar()
        self.payload = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parent.parent/'build'/'setup-bundle'))/'payload'
        ttk.Label(root, text='Tomb Raider Beyond', font=('Segoe UI', 22, 'bold')).pack(anchor='w', padx=28, pady=(24, 6))
        ttk.Label(root, text=f'Version {VERSION}').pack(anchor='w', padx=28)
        self.body = ttk.Frame(root, padding=28); self.body.pack(fill='both', expand=True)
        buttons = ttk.Frame(root, padding=(28, 12)); buttons.pack(fill='x', side='bottom')
        self.back = ttk.Button(buttons, text='Back', command=self.go_back); self.back.pack(side='left')
        self.next = ttk.Button(buttons, text='Next', command=self.go_next); self.next.pack(side='right')
        self.stop = ttk.Button(buttons, text='Cancel', command=self.close); self.stop.pack(side='right', padx=10)
        root.protocol('WM_DELETE_WINDOW', self.close)
        for path in self.source_paths.values():
            path.trace_add('write', self.refresh_next)
        self.show(); root.after(100, self.poll)

    def label(self, text, **options):
        ttk.Label(self.body, text=text, wraplength=630, justify='left', **options).pack(anchor='w', pady=(0, 12))

    def show(self):
        for child in self.body.winfo_children(): child.destroy()
        can_go_back = self.page in ('games', 'assets', 'review') or self.page in TR1_SETTINGS
        self.back.configure(state='normal' if can_go_back else 'disabled')
        updating = self.kind.get() == 'update'
        self.next.configure(text=('Update' if updating else 'Install') if self.page == 'review' else 'Finish' if self.page == 'done' else 'Next', state='disabled' if self.page == 'install' else 'normal')
        headings = {
            'folder': 'Do you want a new installation, or update an existing installation?',
            'games': 'Which Tomb Raider games do you want to import?',
            'assets': 'Import Tomb Raider Assets',
            'review': 'Ready to update' if updating else 'Ready to install',
            'install': 'Updating Tomb Raider Beyond…' if updating else 'Installing and preparing game data…',
            'configure': 'How would you like Tomb Raider to be configured?',
            'done': 'Tomb Raider Beyond is ready',
        }
        if self.page == 'done':
            ttk.Label(self.body, text="Right, let's go adventuring!", style='Heading.TLabel',
                      wraplength=630).pack(anchor='w', pady=(0, 12))
        ttk.Label(self.body, text=headings.get(self.page, self.page), style='Heading.TLabel',
                  wraplength=630).pack(anchor='w', pady=(0, 12))
        if self.page == 'folder':
            for value, label in (('new', 'New installation'), ('update', 'Update existing installation')):
                ttk.Radiobutton(self.body, text=label, variable=self.kind, value=value).pack(anchor='w', pady=4)
            row = ttk.Frame(self.body); row.pack(fill='x', pady=(10, 0))
            ttk.Entry(row, textvariable=self.destination).pack(side='left', fill='x', expand=True)
            ttk.Button(row, text='Browse…', command=self.browse_destination).pack(side='right', padx=(8, 0))
        elif self.page == 'games':
            ttk.Checkbutton(self.body, text='Tomb Raider', variable=self.tr1_enabled,
                            command=self.refresh_next).pack(anchor='w', pady=4)
            ttk.Checkbutton(self.body, text='Tomb Raider II - Coming soon!', state='disabled').pack(anchor='w', pady=4)
        elif self.page == 'assets':
            self.label('Tomb Raider Beyond needs at least one of your copies of Tomb Raider to import the necessary game data. You can import either from a DOS or PlayStation version of the game.')
            self.label('If possible, importing both versions will give you the most options to customise the game to your liking.',
                       font=('Segoe UI', 9, 'bold'))
            for edition, label, prompt in (
                ('DOS', 'DOS', 'Select either the .cue image file of your DOS CD image, or locate GAME.GOG within your Steam or GOG installation.'),
                ('PS1', 'PlayStation', 'Select the .cue image file of your PlayStation CD image'),
            ):
                ttk.Checkbutton(self.body, text=label, variable=self.source_enabled[edition],
                                command=self.show).pack(anchor='w', pady=(0, 5))
                self.label(prompt)
                row = ttk.Frame(self.body); row.pack(fill='x', pady=(0, 12))
                state = 'normal' if self.source_enabled[edition].get() else 'disabled'
                ttk.Entry(row, textvariable=self.source_paths[edition], state=state).pack(side='left', fill='x', expand=True)
                ttk.Button(row, text='Browse…', state=state,
                           command=lambda edition=edition: self.browse_source(edition)).pack(side='right', padx=(8, 0))
        elif self.page == 'review':
            self.label(f'Installation folder:\n{self.destination.get()}')
            if updating:
                self.label('Installation found. Ready to Update.')
                self.label('Runtime files will be updated. Existing game data, settings and saves will be kept.')
            else:
                for kind, selected in self.selections():
                    self.label(f'{"PS1" if kind == "ps1" else "DOS"} source:\n{selected}')
                self.label('Required files found. Ready to Install.')
                space = SPACE_PER_SOURCE_GIB * len(self.selections())
                self.label(f'Allow several minutes for movie conversion. At least {space} GB of free space is required.')
        elif self.page == 'install':
            self.progress = ttk.Progressbar(self.body, maximum=100); self.progress.pack(fill='x', pady=18)
            ttk.Label(self.body, textvariable=self.status, wraplength=630).pack(anchor='w')
        elif self.page == 'configure':
            for value, label in (
                ('Optimal', 'Optimal - Use the best combination of settings for the best experience'),
                ('DOS', 'DOS - Replicate how the game looked in DOS Software mode'),
                ('DOS 3DFX', 'DOS 3DFX - Replicate how the game looked in DOS 3DFX (Glide) mode'),
                ('PS1', 'PlayStation - Replicate how the game looked on the PlayStation'),
                ('Custom', 'Let me decide each setting! - Go step-by-step through each setting'),
            ):
                ttk.Radiobutton(self.body, text=label, variable=self.tr1_preset, value=value).pack(anchor='w', pady=4)
        elif self.page in TR1_SETTINGS:
            if self.page == 'Level Source':
                self.label('Select which version of the game you would like the levels and textures to come from.')
            elif self.page == 'Music and Speech Source':
                self.label('Select which source you would like to take CD audio from.')
            elif self.page == 'FMV Source':
                self.label('Select which source you would like to take pre-rendered videos from.')
            for value, label, description in TR1_SETTINGS[self.page].options:
                ttk.Radiobutton(self.body, text=label, variable=self.custom_values[self.page],
                                value=value).pack(anchor='w')
                self.label(description)
            if self.page == 'Texture Filtering':
                self.label('Unlike later games, TR1 was never designed with Texture Filtering in mind, and therefore the seams between textures are much more noticeable when it is on. Keeping filtering off hides those seams better. Still, this is a matter of preference.')
        else:
            self.label(f'Installed in:\n{self.destination.get()}')
            self.label('Run TombRaiderBeyond.exe from this folder.')
            ttk.Button(self.body, text='Open installation folder', command=lambda: os.startfile(self.destination.get())).pack(anchor='w', pady=8)
            ttk.Button(self.body, text='Launch Tomb Raider Beyond', command=self.launch).pack(anchor='w')
            self.stop.configure(state='disabled')

        self.refresh_next()

    def refresh_next(self, *_):
        if self.page == 'games':
            enabled = self.tr1_enabled.get()
        elif self.page == 'assets':
            try:
                enabled = all(Path(path).is_file() for _, path in self.selections())
            except (ValueError, OSError):
                enabled = False
        else:
            return
        self.next.configure(state='normal' if enabled else 'disabled')

    def selections(self):
        selected = []
        for edition in ('DOS', 'PS1'):
            if self.source_enabled[edition].get():
                path = self.source_paths[edition].get().strip()
                if not path:
                    raise ValueError(f'Choose the {edition} game data file.')
                if edition == 'PS1' and Path(path).suffix.lower() != '.cue':
                    raise ValueError('Select a PS1 CUE file.')
                selected.append(('ps1' if edition == 'PS1' else dos_source_kind(path), path))
        if not selected:
            raise ValueError('Tick at least one source to import.')
        return selected

    def browse_update(self):
        selected = filedialog.askdirectory(title='Select the Tomb Raider Beyond installation folder')
        if selected:
            self.destination.set(selected)

    def browse_destination(self):
        if self.kind.get() == 'update':
            self.browse_update()
            return
        selected = filedialog.askdirectory(title='Choose a parent folder for Tomb Raider Beyond')
        if selected:
            folder = Path(selected)
            if (folder/'Tomb Raider Beyond.exe').is_file() or (folder/'TombRaiderBeyond.exe').is_file() or (folder/'trb-install.json').is_file():
                self.destination.set(str(folder))
            else:
                self.destination.set(str(folder/'Tomb Raider Beyond'))

    def browse_source(self, edition):
        types = [('DOS CUE or GAME.GOG', '*.cue GAME.GOG'), ('CD cue sheet', '*.cue'), ('Steam/GOG disc image', 'GAME.GOG')] if edition == 'DOS' else [('CD cue sheet', '*.cue')]
        selected = filedialog.askopenfilename(title=f'Select your {edition} game data', filetypes=types)
        if selected:
            self.source_paths[edition].set(selected)

    def go_back(self):
        if self.page in TR1_SETTINGS:
            pages = ['configure', *self.custom_pages()]
            self.page = pages[pages.index(self.page) - 1]
            self.show()
            return
        previous = {'games': 'folder', 'assets': 'games',
                    'review': 'folder' if self.kind.get() == 'update' else 'assets'}
        if not self.busy and self.page in previous:
            self.page = previous[self.page]
            self.show()

    def go_next(self):
        if self.page == 'done': self.root.destroy(); return
        try:
            if self.page == 'folder':
                if not self.destination.get().strip(): raise ValueError('Choose an installation folder.')
                path = Path(self.destination.get()).expanduser().resolve()
                if path.exists() and not path.is_dir():
                    raise ValueError('Please choose a folder.')
                self.destination.set(str(path))
                if self.kind.get() == 'update':
                    self.destination.set(str(inspect_installation(path)))
                    self.page = 'review'
                else:
                    if path.is_dir() and any(path.iterdir()):
                        raise ValueError('Choose a new or empty folder for a new installation, or select Update existing installation.')
                    self.page = 'games'
            elif self.page == 'games':
                if not self.tr1_enabled.get():
                    return
                self.page = 'assets'
            elif self.page == 'assets':
                self.root.configure(cursor='watch'); self.root.update_idletasks()
                for kind, selected in self.selections():
                    inspect_source(kind, selected)
                self.page = 'review'
            elif self.page == 'review':
                self.page = 'install'; self.busy = True; self.cancel.clear(); self.show()
                args = (self.kind.get(), [] if self.kind.get() == 'update' else self.selections(), self.destination.get())
                threading.Thread(target=self.worker, args=args, daemon=True).start(); return
            elif self.page == 'configure':
                if self.tr1_preset.get() == 'Custom':
                    self.page = self.custom_pages()[0]
                else:
                    self.save_tr1_preset()
                    self.page = 'done'
            elif self.page in TR1_SETTINGS:
                pages = self.custom_pages()
                if self.page == pages[-1]:
                    self.write_tr1_settings(self.custom_settings())
                    self.page = 'done'
                else:
                    self.page = pages[pages.index(self.page) + 1]
            self.show()
        except Exception as error:
            messagebox.showerror('Cannot continue', str(error))
        finally:
            self.root.configure(cursor='')

    def custom_pages(self):
        both_sources = all(source.get() for source in self.source_enabled.values())
        full_colour = self.custom_values['Colour Depth'].get() == 'Full Colour'
        return [title for title, setting in TR1_SETTINGS.items()
                if (setting.section != 'Game' or both_sources)
                and (title != 'Texture Filtering' or full_colour)]

    def custom_settings(self):
        sections = {'Display': ['visual_preset=Custom'], 'Game': ['source_preset=Custom'], 'Tomb': []}
        sources = [name for name, enabled in self.source_enabled.items() if enabled.get()]
        for title, setting in TR1_SETTINGS.items():
            value = self.custom_values[title].get()
            if setting.section == 'Game' and len(sources) == 1:
                value = sources[0]
            if title == 'Texture Filtering' and self.custom_values['Colour Depth'].get() != 'Full Colour':
                value = '0'
            sections[setting.section].append(f'{setting.key}={value}')
        return ''.join(f'[{name}]\n' + '\n'.join(lines) + '\n' for name, lines in sections.items())

    def save_tr1_preset(self):
        visual = self.tr1_preset.get()
        source = 'DOS' if visual == 'DOS 3DFX' else visual
        # Optimal inherits the runtime's independent level, music and FMV defaults.
        # For other presets the runtime falls back to available imported assets.
        settings = f'[Display]\nvisual_preset={visual}\n[Game]\nsource_preset={source}\n'
        if source != 'Optimal':
            settings += f'level_source={source}\nmusic_speech_source={source}\nfmv_source={source}\n'
        self.write_tr1_settings(settings)

    def write_tr1_settings(self, settings):
        path = Path(self.destination.get())/'Config/TR1/settings.ini'
        with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as temporary:
            staged = Path(temporary.name)
        try:
            staged.write_text(settings, encoding='ascii')
            staged.replace(path)
        finally:
            staged.unlink(missing_ok=True)

    def worker(self, kind, selections, destination):
        try:
            progress = lambda value, text: self.events.put(('progress', value, text))
            if kind == 'update':
                install('update', destination, destination, self.payload, progress, self.cancel)
            else:
                install_sources(selections, destination, self.payload, progress, self.cancel)
            self.events.put(('done',))
        except Exception as error:
            self.events.put(('error', str(error)))

    def poll(self):
        try:
            while True:
                event = self.events.get_nowait()
                if event[0] == 'progress': self.progress['value'] = event[1]; self.status.set(event[2])
                elif event[0] == 'done':
                    self.busy = False
                    self.page = 'configure' if self.kind.get() == 'new' and self.tr1_enabled.get() else 'done'
                    self.show()
                elif event[0] == 'error':
                    self.busy = False; self.page = 'review'; self.show(); self.stop.configure(state='normal')
                    messagebox.showinfo('Installation cancelled' if self.cancel.is_set() else 'Installation failed', event[1])
        except queue.Empty:
            pass
        self.root.after(100, self.poll)

    def close(self):
        if self.busy:
            if messagebox.askyesno('Cancel installation?', 'Stop updating and restore the previous runtime?' if self.kind.get() == 'update' else 'Stop extraction and discard the incomplete installation?'):
                self.cancel.set(); self.status.set('Cancelling safely…'); self.stop.configure(state='disabled')
        else: self.root.destroy()

    def launch(self):
        path = Path(self.destination.get())
        try:
            subprocess.Popen([str(path/'TombRaiderBeyond.exe')], cwd=path)
        except OSError as error:
            messagebox.showerror('Cannot launch Tomb Raider Beyond', str(error))
            return
        self.root.destroy()


if __name__ == '__main__':
    if len(sys.argv) == 6 and sys.argv[1] == '--import-test':
        # Exercises the frozen backend without a desktop, including bundled codecs.
        import json
        payload = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parent.parent/'build'/'setup-bundle'))/'payload'
        try:
            result = install(sys.argv[2], sys.argv[3], sys.argv[4], payload)
            Path(sys.argv[5]).write_text(json.dumps({'ok': True, 'destination': str(result)}))
        except Exception as error:
            Path(sys.argv[5]).write_text(json.dumps({'ok': False, 'error': str(error)}))
            raise SystemExit(1)
    else:
        root = tk.Tk(); Wizard(root); root.mainloop()
