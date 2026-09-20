#!/usr/bin/env python3
"""Package the locally verified M6 candidate, preserving all M5 controls."""

import argparse
import hashlib
import json
import re
import subprocess
import zipfile
from pathlib import Path, PurePosixPath

import pocket_firmware_pair as pair
import pocket_m5_audio_package as baseline
import pocket_package as shared

CORE_ID = 'RPCMP.AlbumPlayer'
PLATFORM_ID = 'rpcmp_player'
VERSION = '0.11.0-m6-preview-r3'
# Candidate identity gates, not hardware-acceptance claims.
APP_SHA256 = 'b6f8ac38edb6a3b1dfff651526c45652c4482d293215aa4179d5323437d0ca48'
MIF_SHA256 = 'c479524bb380900a8eb4b0a4433b2513cb4c5e039568fef3d47776c2be002e60'
OS_SHA256 = 'fcd131fa43ca7f953576b69717c33f77cbcf07d8149e182a6e917066bf6f2080'
RBF_SHA256 = 'd5d6f7945fe69f17c56e1e9c15cf2d7ecb30fc4b4382e2b79c10d33575d95043'
FONT_NOTICES = ('COPYING', 'OFL-1.1.txt', 'README.md')


def definitions():
    values = baseline.definitions()
    core = values['core.json']['core']
    core['metadata'].update(platform_ids=[PLATFORM_ID], shortname='AlbumPlayer',
                            description='RPCMP M6 album player preview', version=VERSION,
                            date_release='2026-09-20')
    core['framework']['version_required'] = '2.2'
    slots = values['data.json']['data']['data_slots']
    slots[0]['name'] = 'Album Player'
    slots.append({'id': 4, 'name': 'Music Library', 'required': False, 'parameters': 8,
                  'extensions': ['rpcmlib'], 'deferload': True, 'size_maximum': 32 * 1024 * 1024})
    values['input.json']['input']['controllers'] = [{'type': 'default', 'mappings': [
        {'id': number, 'name': name, key: True} for number, name, key in (
            (0, 'Select / Play-Pause', 'pad_btn_a'), (1, 'Stop / Back', 'pad_btn_b'),
            (4, 'Previous Track', 'pad_trig_l'), (5, 'Next Track', 'pad_trig_r'))]}]
    # Slot numbers are an ABI with pinned OS video.c and core_top.v. In
    # particular full resolution is slot 7, not a newly appended slot 1.
    modes = ((320, 240, 4, 3), (320, 200, 4, 3), (320, 224, 10, 7), (320, 256, 5, 4),
             (320, 288, 10, 9), (400, 300, 4, 3), (256, 240, 4, 3), (640, 480, 4, 3))
    values['video.json']['video']['scaler_modes'] = [
        dict(width=w, height=h, aspect_w=aw, aspect_h=ah, rotation=0, mirror=0)
        for w, h, aw, ah in modes]
    return values


def expected_paths():
    core = PurePosixPath('Cores') / CORE_ID
    common = PurePosixPath('Assets') / PLATFORM_ID / 'common'
    return {*(core / name for name in definitions()), core / 'loader.bin', core / 'rpcmp.rbf_r',
            *(core / 'LICENSES' / 'unifont' / name for name in FONT_NOTICES),
            *(common / name for name in ('os.bin', 'player.elf', 'player.ini', 'music.rpcmlib')),
            PurePosixPath('Assets') / PLATFORM_ID / CORE_ID / 'player.json',
            PurePosixPath('Platforms') / f'{PLATFORM_ID}.json'}


def candidate_files(loader, rbf, os_image, elf, library, notices):
    core = PurePosixPath('Cores') / CORE_ID
    common = PurePosixPath('Assets') / PLATFORM_ID / 'common'
    files = {core / name: shared.json_bytes(value) for name, value in definitions().items()}
    files.update({core / 'loader.bin': loader, core / 'rpcmp.rbf_r': shared.reverse_rbf_bits(rbf),
                  common / 'os.bin': os_image, common / 'player.elf': elf,
                  common / 'music.rpcmlib': library,
                  common / 'player.ini': b'[os]\nELF=player.elf\nVARIANT=rpcmp\n'})
    files.update({core / 'LICENSES' / 'unifont' / name: notices[name] for name in FONT_NOTICES})
    files[PurePosixPath('Assets') / PLATFORM_ID / CORE_ID / 'player.json'] = shared.json_bytes(
        {'instance': {'magic': shared.MAGIC, 'data_slots': [
            {'id': i, 'filename': name} for i, name in enumerate(
                ('os.bin', 'player.ini', 'player.elf', 'music.rpcmlib'), 1)]}})
    files[PurePosixPath('Platforms') / f'{PLATFORM_ID}.json'] = shared.json_bytes(
        {'platform': {'category': 'Computer', 'name': 'RPCMP Album Player',
                      'year': 2026, 'manufacturer': 'RPCMP'}})
    return files


def write_package(files, output, archive):
    if (output.exists() or output.is_symlink() or archive.exists() or archive.is_symlink() or
            archive.with_suffix('.evidence.json').exists()):
        raise ValueError('candidate output already exists; preserve it')
    if set(files) != expected_paths():
        raise ValueError('candidate allowlist mismatch')
    for path, contents in files.items():
        if path.suffix == '.json':
            data = json.loads(contents)
            if len(data) != 1 or (path.parts[0] != 'Platforms' and
                                  next(iter(data.values())).get('magic') != shared.MAGIC):
                raise ValueError('APF JSON root/magic mismatch')
        shared.write_file(output, path, contents)
    with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zipped:
        for path, contents in sorted(files.items()):
            info = zipfile.ZipInfo(path.as_posix(), (2026, 9, 20, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            zipped.writestr(info, contents)
    with zipfile.ZipFile(archive) as zipped:
        if sorted(zipped.namelist()) != sorted(p.as_posix() for p in files):
            raise ValueError('archive allowlist mismatch')
        for path, contents in files.items():
            if zipped.read(path.as_posix()) != contents or (output / path).read_bytes() != contents:
                raise ValueError('candidate readback mismatch')
    return {p.as_posix(): {'bytes': len(b), 'sha256': hashlib.sha256(b).hexdigest()}
            for p, b in sorted(files.items())}


def verify_app(elf):
    if shared.sha256(elf) != APP_SHA256:
        raise ValueError('application identity is not the verified M6 link')
    sections, _ = pair.elf_sections(elf.read_bytes())
    budget = json.loads(elf.with_name('budget.json').read_text(encoding='utf-8'))
    measured = {
        'text_bytes': sum(sections[name][1] for name in ('.text', '.eh_frame')),
        'data_bytes': sum(sections[name][1] for name in ('.data', '.init_array')),
        'bss_bytes': sections['.bss'][1],
    }
    if any(budget[k] != v for k, v in measured.items()):
        raise ValueError('application budget does not match ELF sections')
    if (budget['static_bytes'] != sum(measured.values()) or budget['static_bytes'] > 56623104 or
            budget['data_bytes'] > 4096 or budget['dynamic_stack_frames'] or
            not 0 < budget['conservative_stack_bound_bytes'] <= 524288):
        raise ValueError('application budget failed')
    return budget


def verify_image(fpga, firmware):
    image = fpga / 'output_files/ap_core.rbf'
    if shared.sha256(image) != RBF_SHA256:
        raise ValueError('RBF identity is not the verified M6 build')
    pairing = pair.verify(firmware / 'firmware.elf', fpga / 'firmware.mif', firmware / 'os.bin')
    if pairing['mif_sha256'] != MIF_SHA256 or pairing['os_sha256'] != OS_SHA256:
        raise ValueError('firmware identity is not the verified M6 pair')
    evidence = {}
    for suffix in ('map.rpt', 'fit.rpt', 'sta.rpt', 'asm.rpt', 'flow.rpt'):
        report = fpga / 'output_files' / f'ap_core.{suffix}'
        text = report.read_text(encoding='utf-8', errors='replace')
        success = (re.search(r'Flow Status\s*;\s*Successful', text) if suffix == 'flow.rpt'
                   else 'successful. 0 errors' in text)
        if not success or 'Critical Warning (127003)' in text or 'setting all initial values to 0' in text:
            raise ValueError(f'invalid Quartus completion: {suffix}')
        if suffix == 'map.rpt' and str((fpga / 'firmware.mif').resolve()).replace('\\', '/') not in text:
            raise ValueError('mapped ROM path differs from the paired ROM')
        evidence[suffix] = shared.sha256(report)
    return image, pairing, evidence


def build(repo, sdk, elf, fpga, firmware, library, preflight, name):
    if not re.fullmatch(r'm6-player-[a-z0-9-]+', name):
        raise ValueError('use a dedicated m6-player-<name> output')
    output = repo / 'out/build' / name
    archive = output.with_suffix('.zip')
    image, pairing, reports = verify_image(fpga, firmware)
    budget = verify_app(elf)
    revision = subprocess.check_output(['git', '-C', str(sdk), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != shared.SDK_REVISION:
        raise ValueError('SDK revision mismatch')
    manifest = shared.parse_manifest(sdk / 'runtime/MANIFEST')
    loader = shared.verify_runtime_file(sdk / 'runtime', manifest, 'pocket/loader.bin')
    if not 80 <= library.stat().st_size <= 32 * 1024 * 1024:
        raise ValueError('library outside M6 limits')
    checked = subprocess.run([str(preflight), str(library)], capture_output=True, text=True, check=True)
    if not re.fullmatch(r'albums=[1-9][0-9]* tracks=[1-9][0-9]*\nresult=PASS\n', checked.stdout):
        raise ValueError('Core library preflight did not pass')
    files = candidate_files(loader.read_bytes(), image.read_bytes(), (firmware / 'os.bin').read_bytes(),
                            elf.read_bytes(), library.read_bytes(),
                            {n: (repo / 'third_party/unifont' / n).read_bytes() for n in FONT_NOTICES})
    members = write_package(files, output, archive)
    evidence = {'schema': 1, 'status': 'local M6 candidate; hardware acceptance pending',
                'distribution': 'Do not redistribute libraries containing private user music.',
                'settings': 'volatile; AlbumOrder/Default on launch; no autoplay',
                'firmware_pair': pairing, 'quartus_reports': reports, 'app_budget': budget,
                'library_preflight': checked.stdout.splitlines(), 'artifacts': members,
                'native_rbf_sha256': shared.sha256(image),
                'zip': {'bytes': archive.stat().st_size, 'sha256': shared.sha256(archive)}}
    archive.with_suffix('.evidence.json').write_bytes(shared.json_bytes(evidence))
    return evidence


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for argument in ('repo', 'sdk', 'elf', 'fpga', 'firmware', 'library', 'preflight'):
        parser.add_argument(f'--{argument}', type=Path, required=True)
    parser.add_argument('--name', default='m6-player-candidate')
    args = parser.parse_args()
    result = build(*(getattr(args, name).resolve() for name in
                     ('repo', 'sdk', 'elf', 'fpga', 'firmware', 'library', 'preflight')), args.name)
    print(f"zip_sha256={result['zip']['sha256']}\nresult=PASS")
