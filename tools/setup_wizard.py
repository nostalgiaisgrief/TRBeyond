"""Portable Tomb Raider Beyond setup wizard; freeze with build-setup.ps1."""
from pathlib import Path
import os
import queue
import re
import subprocess
import sys
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

from setup_import import install, install_sources, dos_source_kind, inspect_source, inspect_installation


VERSION_PATH = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parent.parent)) / 'version.txt'
VERSION = VERSION_PATH.read_text(encoding='utf-8-sig').strip()
if not re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)', VERSION) or any(int(v) > 65535 for v in VERSION.split('.')):
    raise ValueError('Invalid version.txt: expected major.minor.patch, each between 0 and 65535.')


class Wizard:
    def __init__(self, root):
        self.root = root
        root.title(f'Tomb Raider Beyond — Version {VERSION}')
        root.geometry('720x700'); root.minsize(720, 700)
        style = ttk.Style(root)
        style.configure('Step.TLabel', font=('Segoe UI', 9), foreground='#666666')
        style.configure('Heading.TLabel', font=('Segoe UI', 17, 'bold'))
        style.configure('StepCurrent.TFrame', background='#526B7A')
        style.configure('StepOther.TFrame', background='#D5DADD')
        self.page = 0
        self.busy = False
        self.cancel = threading.Event()
        self.events = queue.Queue()
        self.destination = tk.StringVar(value=str(Path.home()/'Games'/'Tomb Raider Beyond'))
        self.kind = tk.StringVar(value='new')
        self.source_paths = {edition: tk.StringVar() for edition in ('DOS', 'PS1')}
        self.source_enabled = {edition: tk.BooleanVar(value=edition == 'DOS') for edition in ('DOS', 'PS1')}
        self.status = tk.StringVar()
        self.payload = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parent.parent/'build'/'setup-bundle'))/'payload'
        ttk.Label(root, text='Tomb Raider Beyond', font=('Segoe UI', 22, 'bold')).pack(anchor='w', padx=28, pady=(24, 6))
        ttk.Label(root, text=f'Version {VERSION} • Portable installation • Bring your own original game data').pack(anchor='w', padx=28)
        self.body = ttk.Frame(root, padding=28); self.body.pack(fill='both', expand=True)
        buttons = ttk.Frame(root, padding=(28, 12)); buttons.pack(fill='x', side='bottom')
        self.back = ttk.Button(buttons, text='Back', command=self.go_back); self.back.pack(side='left')
        self.next = ttk.Button(buttons, text='Next', command=self.go_next); self.next.pack(side='right')
        self.stop = ttk.Button(buttons, text='Cancel', command=self.close); self.stop.pack(side='right', padx=10)
        root.protocol('WM_DELETE_WINDOW', self.close)
        self.show(); root.after(100, self.poll)

    def label(self, text):
        ttk.Label(self.body, text=text, wraplength=630, justify='left').pack(anchor='w', pady=(0, 12))

    def show(self):
        for child in self.body.winfo_children(): child.destroy()
        self.back.configure(state='normal' if self.page in (1, 2) else 'disabled')
        updating = self.kind.get() == 'update'
        self.next.configure(text=('Update' if updating else 'Install') if self.page == 2 else 'Finish' if self.page == 4 else 'Next', state='disabled' if self.page == 3 else 'normal')
        headings = ('Choose an installation folder', 'Locate your original game data',
                    'Ready to update' if updating else 'Ready to install',
                    'Updating Tomb Raider Beyond…' if updating else 'Installing and preparing game data…',
                    'Tomb Raider Beyond is ready')
        if self.page < 3:
            ttk.Label(self.body, text=f'Step {self.page + 1} of 3',
                      style='Step.TLabel').pack(anchor='w', pady=(0, 5))
        if self.page == 4:
            ttk.Label(self.body, text="Right, let's go adventuring!", style='Heading.TLabel',
                      wraplength=630).pack(anchor='w', pady=(0, 12))
        ttk.Label(self.body, text=headings[self.page], style='Heading.TLabel',
                  wraplength=630).pack(anchor='w', pady=(0, 12))
        if self.page < 3:
            indicator = ttk.Frame(self.body)
            indicator.pack(fill='x', pady=(0, 24))
            for index in range(3):
                indicator.columnconfigure(index, weight=1, uniform='step')
                ttk.Frame(indicator, height=3,
                          style='StepCurrent.TFrame' if index == self.page else 'StepOther.TFrame').grid(
                              row=0, column=index, sticky='ew', padx=(0, 5) if index < 2 else 0)
        if self.page == 0:
            self.label('Choose a new installation folder, or the folder you want to update.')
            row = ttk.Frame(self.body); row.pack(fill='x')
            ttk.Entry(row, textvariable=self.destination).pack(side='left', fill='x', expand=True)
            ttk.Button(row, text='Browse…', command=self.browse_destination).pack(side='right', padx=(8, 0))
        elif self.page == 1:
            ttk.Radiobutton(self.body,
                            text='New installation - Import game data from one of your owned copies',
                            variable=self.kind, value='new', command=self.show).pack(anchor='w', pady=4)
            ttk.Radiobutton(self.body,
                            text="Update existing installation - Don't import game assets",
                            variable=self.kind, value='update', command=self.show).pack(anchor='w', pady=(4, 14))
            if updating:
                row = ttk.Frame(self.body); row.pack(fill='x', pady=(0, 12))
                ttk.Entry(row, textvariable=self.destination).pack(side='left', fill='x', expand=True)
                ttk.Button(row, text='Browse…', command=self.browse_update).pack(side='right', padx=(8, 0))
                self.label('Choose the Tomb Raider Beyond folder. Existing game data, settings and saves will be kept.')
            else:
                self.label('Tick the sources you want to import.')
                self.label('Importing from multiple sources allows you to mix and match assets between versions within Tomb Raider Beyond - For example: DOS audio with PS1 videos')
                for edition, label in (('DOS', 'DOS / Steam / GOG'), ('PS1', 'PS1')):
                    ttk.Checkbutton(self.body, text=label, variable=self.source_enabled[edition],
                                    command=self.show).pack(anchor='w', pady=(0, 5))
                    row = ttk.Frame(self.body); row.pack(fill='x', pady=(0, 12))
                    state = 'normal' if self.source_enabled[edition].get() else 'disabled'
                    ttk.Entry(row, textvariable=self.source_paths[edition], state=state).pack(side='left', fill='x', expand=True)
                    ttk.Button(row, text='Browse…', state=state,
                               command=lambda edition=edition: self.browse_source(edition)).pack(side='right', padx=(8, 0))
                ttk.Checkbutton(self.body, text='Remaster — coming later', state='disabled').pack(anchor='w', pady=(0, 12))
                self.label('DOS: choose a .cue file or GAME.GOG. PS1: choose a .cue file.')
        elif self.page == 2:
            self.label(f'Installation folder:\n{self.destination.get()}')
            if updating:
                self.label('Installation found. Ready to Update.')
                self.label('Runtime files will be updated. Existing game data, settings and saves will be kept.')
            else:
                for kind, selected in self.selections():
                    self.label(f'{"PS1" if kind == "ps1" else "DOS"} source:\n{selected}')
                self.label('Required files found. Ready to Install.')
                space = sum(10 if kind == 'ps1' else 2 for kind, _ in self.selections())
                self.label(f'Allow several minutes for movie conversion. At least {space} GB of free space is required.')
        elif self.page == 3:
            self.progress = ttk.Progressbar(self.body, maximum=100); self.progress.pack(fill='x', pady=18)
            ttk.Label(self.body, textvariable=self.status, wraplength=630).pack(anchor='w')
        else:
            self.label(f'Installed in:\n{self.destination.get()}')
            self.label('Run TombRaiderBeyond.exe from this folder.')
            ttk.Button(self.body, text='Open installation folder', command=lambda: os.startfile(self.destination.get())).pack(anchor='w', pady=8)
            ttk.Button(self.body, text='Launch Tomb Raider Beyond', command=self.launch).pack(anchor='w')
            self.stop.configure(state='disabled')

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
        if not self.busy: self.page -= 1; self.show()

    def go_next(self):
        if self.page == 4: self.root.destroy(); return
        try:
            if self.page == 0:
                if not self.destination.get().strip(): raise ValueError('Choose an installation folder.')
                path = Path(self.destination.get()).expanduser().resolve()
                if path.exists() and not path.is_dir():
                    raise ValueError('Please choose a folder.')
                self.destination.set(str(path))
                if path.is_dir() and any(path.iterdir()):
                    inspect_installation(path)
                    self.kind.set('update')
            elif self.page == 1:
                self.root.configure(cursor='watch'); self.root.update_idletasks()
                if self.kind.get() == 'update':
                    self.destination.set(str(inspect_installation(self.destination.get())))
                else:
                    target = Path(self.destination.get())
                    if target.exists() and any(target.iterdir()):
                        raise ValueError('Choose a new or empty folder in Step 1 for a new installation, or select Update existing installation.')
                    for kind, selected in self.selections():
                        inspect_source(kind, selected)
            elif self.page == 2:
                self.page = 3; self.busy = True; self.cancel.clear(); self.show()
                args = (self.kind.get(), [] if self.kind.get() == 'update' else self.selections(), self.destination.get())
                threading.Thread(target=self.worker, args=args, daemon=True).start(); return
            self.page += 1; self.show()
        except Exception as error:
            messagebox.showerror('Cannot continue', str(error))
        finally:
            self.root.configure(cursor='')

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
                elif event[0] == 'done': self.busy = False; self.page = 4; self.show()
                elif event[0] == 'error':
                    self.busy = False; self.page = 2; self.show(); self.stop.configure(state='normal')
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
        subprocess.Popen([str(path/'TombRaiderBeyond.exe')], cwd=path)


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
