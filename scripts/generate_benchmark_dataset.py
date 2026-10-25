#!/usr/bin/env python3
"""
Benchmark dataset generator for QG performance testing.

Creates realistic video fixtures on NVMe SSD for performance testing.
Generates files with:
- Realistic sizes (using sparse files to save space)
- Realistic filenames
- Actual video content (minimal valid MP4 containers) where possible
- Varied modification times
- Supported media formats
"""

import os
import sys
import random
import string
import subprocess
import tempfile
import time
from pathlib import Path

# Supported media formats from DESIGN.md
FORMATS = ['mp4', 'mkv', 'webm', 'avi', 'mov', 'm4v', 'mpeg', 'mpg', 'ts', 'm2ts', 'mts', 'ogv', '3gp', 'flv', 'wmv']

# Realistic filename prefixes
PREFIXES = [
    'video', 'clip', 'recording', 'movie', 'footage', 'capture',
    'session', 'take', 'scene', 'shot', 'cam', 'dashcam',
    'gopro', 'drone', 'phone', 'screen', 'stream', 'vlog'
]

SUFFIXES = [
    '', '_final', '_edit', '_raw', '_cut', '_color', '_grade',
    '_v2', '_v3', '_final_v2', '_master', '_proxy', '_render'
]

def random_string(length=8):
    return ''.join(random.choices(string.ascii_lowercase + string.digits, k=length))

def generate_filename(index, total):
    """Generate a realistic video filename."""
    prefix = random.choice(PREFIXES)
    suffix = random.choice(SUFFIXES)
    fmt = random.choice(FORMATS)
    
    # Various naming patterns
    patterns = [
        f"{prefix}_{index:06d}{suffix}.{fmt}",
        f"{prefix}-{random_string(6)}{suffix}.{fmt}",
        f"{prefix}_{time.strftime('%Y%m%d')}_{index:04d}{suffix}.{fmt}",
        f"VID_{time.strftime('%Y%m%d')}_{random_string(8)}.{fmt}",
        f"clip_{index:05d}_{random_string(4)}.{fmt}",
    ]
    pattern = random.choice(patterns)
    return pattern

def create_sparse_file(path, size_bytes):
    """Create a sparse file of the given size."""
    with open(path, 'wb') as f:
        f.seek(size_bytes - 1)
        f.write(b'\0')

def create_minimal_mp4(path):
    """Create a minimal valid MP4 file (ftyp + moov atoms)."""
    # Minimal MP4: ftyp + moov (empty)
    # This is a valid but empty MP4 container
    ftyp = bytes([
        0x00, 0x00, 0x00, 0x18,  # size: 24
        0x66, 0x74, 0x79, 0x70,  # 'ftyp'
        0x69, 0x73, 0x6F, 0x6D,  # 'isom'
        0x00, 0x00, 0x02, 0x00,  # minor version
        0x69, 0x73, 0x6F, 0x6D,  # 'isom'
        0x69, 0x73, 0x6F, 0x32,  # 'iso2'
        0x61, 0x76, 0x63, 0x31,  # 'avc1'
        0x6D, 0x70, 0x34, 0x31,  # 'mp41'
    ])
    # Minimal moov atom (empty movie)
    moov = bytes([
        0x00, 0x00, 0x00, 0x08,  # size: 8
        0x6D, 0x6F, 0x6F, 0x76,  # 'moov'
    ])
    # mdat atom (empty media data)
    mdat = bytes([
        0x00, 0x00, 0x00, 0x08,  # size: 8
        0x6D, 0x64, 0x61, 0x74,  # 'mdat'
    ])
    
    with open(path, 'wb') as f:
        f.write(ftyp)
        f.write(moov)
        f.write(mdat)

def create_video_file(path, fmt, size_category='small'):
    """Create a video file with realistic size."""
    # Size categories in bytes - keep logical sizes small for benchmark purposes
    # Actual disk usage will be minimal due to sparse file creation
    sizes = {
        'tiny': (512, 4*1024),        # 512B - 4KB
        'small': (4*1024, 64*1024),   # 4KB - 64KB
        'medium': (64*1024, 512*1024), # 64KB - 512KB
        'large': (512*1024, 2*1024*1024), # 512KB - 2MB
    }
    
    min_size, max_size = sizes.get(size_category, sizes['small'])
    size = random.randint(min_size, max_size)
    
    if fmt == 'mp4':
        # Create minimal valid MP4 then pad to size
        create_minimal_mp4(path)
        current_size = os.path.getsize(path)
        if current_size < size:
            with open(path, 'ab') as f:
                f.seek(size - 1)
                f.write(b'\0')
    else:
        # For other formats, create sparse file
        create_sparse_file(path, size)
    
    # Set random modification time within last year
    now = time.time()
    random_mtime = now - random.randint(0, 365*24*3600)
    os.utime(path, (random_mtime, random_mtime))

def generate_dataset(base_path, count, batch_size=1000):
    """Generate a dataset of video files."""
    base_path = Path(base_path)
    base_path.mkdir(parents=True, exist_ok=True)
    
    print(f"Generating {count} video files in {base_path}")
    
    for i in range(count):
        if i % batch_size == 0 and i > 0:
            print(f"  Generated {i}/{count} files...")
        
        filename = generate_filename(i, count)
        filepath = base_path / filename
        
        # Choose format and size category
        fmt = random.choice(FORMATS)
        size_cat = random.choices(
            ['tiny', 'small', 'medium', 'large'],
            weights=[0.1, 0.5, 0.3, 0.1]
        )[0]
        
        create_video_file(base_path / filename, fmt, size_cat)
    
    print(f"Done! Generated {count} files in {base_path}")

def verify_dataset(path, expected_count):
    """Verify the generated dataset."""
    path = Path(path)
    files = list(path.glob('*'))
    actual_count = len(files)
    
    print(f"Verification: expected {expected_count}, found {actual_count}")
    
    if actual_count != expected_count:
        print(f"WARNING: Count mismatch!")
        return False
    
    # Check formats
    format_counts = {}
    total_size = 0
    for f in files:
        ext = f.suffix.lower().lstrip('.')
        format_counts[ext] = format_counts.get(ext, 0) + 1
        total_size += f.stat().st_size
    
    print(f"Format distribution: {format_counts}")
    print(f"Total size: {total_size / (1024**3):.2f} GB")
    print(f"Average size: {total_size / actual_count / 1024:.1f} KB")
    
    return True

if __name__ == '__main__':
    import argparse
    
    parser = argparse.ArgumentParser(description='Generate benchmark video dataset')
    parser.add_argument('output_dir', help='Output directory')
    parser.add_argument('--count', type=int, default=50000, help='Number of files to generate')
    parser.add_argument('--verify-only', action='store_true', help='Only verify existing dataset')
    parser.add_argument('--batch-size', type=int, default=1000, help='Batch size for progress reporting')
    
    args = parser.parse_args()
    
    if args.verify_only:
        verify_dataset(args.output_dir, args.count)
    else:
        generate_dataset(args.output_dir, args.count, args.batch_size)
        verify_dataset(args.output_dir, args.count)