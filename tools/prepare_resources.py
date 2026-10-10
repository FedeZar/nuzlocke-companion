#!/usr/bin/env python3
"""Prepare local resources from your own IRDI ROM and public PokeAPI data.

The ROM is read-only. No save files, nicknames, trainer IDs or run data are read.
Requires Python 3.9+; uses the standard library only.
"""
import argparse
import csv
import io
import json
import struct
import unicodedata
import urllib.request
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
from rom_reader import ROM, text_entries, u16, u32

TYPES = ['Normal','Fighting','Flying','Poison','Ground','Rock','Bug','Ghost','Steel','Fire','Water','Grass','Electric','Psychic','Ice','Dragon','Dark']
BASE = 'https://raw.githubusercontent.com/PokeAPI/pokeapi/master/data/v2/csv/'
METHODS = ['Grass / cave','Dark grass','Rustling grass','Surf','Rippling water · Surf','Fishing','Rippling water · Fishing']

def download(url, cache):
    if cache.exists():
        return cache.read_bytes()
    req = urllib.request.Request(url, headers={'User-Agent': 'NuzlockeCompanion-resource-preparer'})
    with urllib.request.urlopen(req, timeout=45) as response:
        data = response.read()
    cache.parent.mkdir(parents=True, exist_ok=True)
    temporary = cache.with_suffix(cache.suffix + '.tmp')
    temporary.write_bytes(data)
    temporary.replace(cache)
    return data

def table(name, cache):
    return list(csv.DictReader(io.StringIO(download(BASE + name + '.csv', cache / (name + '.csv')).decode('utf-8'))))

def english(rows, id_key):
    return {int(r[id_key]): r['name'] for r in rows if r['local_language_id'] == '9'}

def normalize(value):
    return ''.join(c for c in unicodedata.normalize('NFKD', value).lower() if c.isalnum() and not unicodedata.combining(c))

def fonts(rom):
    result = []
    for data in rom.narc('a/0/2/3')[:2]:
        block = data.index(b'PLGC')
        w, h, cell_size = data[block+8], data[block+9], u16(data, block+10)
        if data[block+14] != 2:
            raise ValueError('Unsupported font depth')
        cells = data[block+16:block+u32(data, block+4)]
        mappings = {}
        # CMAP blocks form a linked list; offsets point eight bytes into a block.
        pos = u32(data, 16+24) - 8
        visited = set()
        while 0 <= pos < len(data) and pos not in visited and data[pos:pos+4] == b'PAMC':
            visited.add(pos)
            lo, hi, method = u16(data,pos+8), u16(data,pos+10), u16(data,pos+12)
            if method == 0:
                first = u16(data,pos+20)
                mappings.update((ch, first+ch-lo) for ch in range(lo,hi+1))
            elif method == 1:
                mappings.update((ch, u16(data,pos+20+2*(ch-lo))) for ch in range(lo,hi+1))
            elif method == 2:
                for i in range(u16(data,pos+20)):
                    ch, index = struct.unpack_from('<HH', data, pos+22+i*4)
                    mappings[ch] = index
            else:
                raise ValueError('Unsupported CMAP mapping')
            following = u32(data,pos+16)
            if not following:
                break
            pos = following - 8
        glyphs = {}
        for ch, index in mappings.items():
            # Latin characters plus common UI and Pokémon symbols.
            if not (32 <= ch <= 383 or ch in [0x2026,0x2190,0x2191,0x2192,0x2193,0x2605,0x2640,0x2642]):
                continue
            if index == 65535:
                continue
            cell = cells[index*cell_size:(index+1)*cell_size]
            if len(cell) != cell_size:
                raise ValueError('Invalid glyph index')
            packed = cell[3:]
            pixels = ''.join(str((packed[i//4] >> (6-2*(i%4))) & 3) for i in range(w*h))
            glyphs[str(ch)] = {'left':cell[0], 'advance':cell[2], 'pixels':pixels}
        if '63' not in glyphs:
            raise ValueError('Font character map could not be decoded')
        result.append({'width':w, 'height':h, 'glyphs':glyphs})
    return {'fonts':result}

def build(rom, cache):
    texts = rom.narc('a/0/0/2')
    italian_locations = text_entries(texts[109])
    species_names = english(table('pokemon_species_names',cache),'pokemon_species_id')
    move_names = english(table('move_names',cache),'move_id')
    ability_names = english(table('ability_names',cache),'ability_id')
    item_names = english(table('item_names',cache),'item_id')
    location_rows = table('location_names',cache)
    location_en = english(location_rows,'location_id')
    location_pairs = {normalize(r['name']):location_en.get(int(r['location_id'])) for r in location_rows if r['local_language_id']=='8'}
    locations = []
    for i,name in enumerate(italian_locations):
        value = location_pairs.get(normalize(name))
        if name.startswith('Percorso '):
            value = 'Route ' + name.split(' ',1)[1]
        locations.append(value or ('Unknown area' if not name else f'Area {i}'))
    # Gates/interior map names not exposed as separate PokeAPI locations.
    special = {0:'Unknown area',1:'Mystery Zone',2:'Faraway place',3:'Link trade',36:'Pokémon World Tournament',46:'Royal Unova',47:'Gear Station',48:'Battle Subway',49:'Musical Theater',52:'Unity Tower',58:'Storage',60:'Poké Transfer Lab',69:'Entralink',73:'Trial Chamber',74:'Guidance Chamber',75:'Entree Forest',106:'Accumula Gate',107:'Undella Gate',108:'Nacrene Gate',109:'Castelia Gate',110:'Nimbasa Gate',111:'Opelucid Gate',112:'Black Gate',113:'White Gate',114:'Bridge Gate',115:'Route Gate',120:'Pokéstar Studios',138:'Unknown area',139:'White Treehollow',140:'Black Tower',143:'Hidden Grotto',145:'Virbank Gate',146:'Aspertia Gate',148:'Medal Office',150:'Rock Peak Chamber',151:'Iceberg Chamber',152:'Iron Chamber'}
    for i,name in special.items():
        if i < len(locations):locations[i] = name
    personal = rom.narc('a/0/1/6')[:650]
    parents = list(range(650))
    def root(i):
        while parents[i] != i:i = parents[i]
        return i
    for sid,raw in enumerate(rom.narc('a/0/1/9')[:650]):
        for offset in range(0,min(len(raw),42),6):
            method,_,target = struct.unpack_from('<HHH',raw,offset)
            if method and 0 < target < 650:parents[root(target)] = root(sid)
    species = [{'id':i,'name':species_names.get(i,'Unknown'),'types':list(p[6:8]),'base_stats':list(p[:6]),'growth':p[21],'family':root(i)} for i,p in enumerate(personal)]
    moves = []
    # English move descriptions from the Gen V version group, not current mechanics.
    prose = {int(r['move_id']):' '.join(r['flavor_text'].split()) for r in table('move_flavor_text',cache) if r['language_id']=='9' and r['version_group_id']=='14'}
    for i,raw in enumerate(rom.narc('a/0/2/1')[:560]):
        moves.append({'id':i,'name':move_names.get(i,'Unknown'),'type':TYPES[raw[0]],'category':['Status','Physical','Special'][raw[2]],'power':raw[3],'accuracy':raw[4],'base_pp':raw[5],'priority':struct.unpack_from('<b',raw,6)[0],'description':prose.get(i,'')})
    abilities = [ability_names.get(i,'None' if i==0 else f'Ability {i}') for i in range(len(text_entries(texts[374])))]
    items = ['None' if i==0 else f'Item {i}' for i in range(len(text_entries(texts[64])))]
    for r in table('item_game_indices',cache):
        if r['generation_id']=='5':
            index = int(r['game_index'])
            if 0 < index < len(items):items[index] = item_names.get(int(r['item_id']),items[index])
    named_locations = json.loads(Path(__file__).with_name('locations.json').read_text())
    for index,name in named_locations.items():
        locations[int(index)] = name
    raw_maps = rom.narc('a/0/1/2')[0]
    maps = [{'id':i,'name_id':raw_maps[i*48+26],'name':locations[raw_maps[i*48+26]],'name_it':italian_locations[raw_maps[i*48+26]],'wild':raw_maps[i*48+20]} for i in range(len(raw_maps)//48)]
    encounters = []
    for raw in rom.narc('a/1/2/7'):
        seasons = []
        for start in range(0,len(raw),232):
            chunk = raw[start:start+232]
            if len(chunk)!=232:raise ValueError('Incomplete encounter table')
            methods=[];pos=8
            for k,count in enumerate([12,12,12,5,5,5,5]):
                slots=[]
                for j in range(count):
                    sid,lo,hi=struct.unpack_from('<HBB',chunk,pos);pos+=4
                    slots.append({'species_id':sid&0x7ff,'form':sid>>11,'min':lo,'max':hi,'chance':([20,20,10,10,10,10,5,5,4,4,1,1] if count==12 else [60,30,5,4,1])[j]})
                if chunk[k]:methods.append({'method':METHODS[k],'rate':chunk[k],'slots':slots})
            seasons.append(methods)
        encounters.append(seasons)
    bosses=json.loads(Path(__file__).with_name('bosses.json').read_text())
    for boss in bosses:
        for mon in boss['team']:mon['name']=species[mon['species_id']]['name']
    return {'game_code':rom.code,'species':species,'moves':moves,'abilities':abilities,'items':items,'map_names':locations,'maps':maps,'encounters':encounters,'bosses':bosses}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rom',type=Path,required=True,help='Your own Italian White 2 .nds file (IRDI)')
    parser.add_argument('--out',type=Path,default=Path(__file__).resolve().parents[1]/'resources/nuzlocke')
    parser.add_argument('--cache',type=Path,default=Path.home()/'.cache/nuzlocke-companion')
    parser.add_argument('--no-sprites',action='store_true',help='Skip optional PokeAPI sprite downloads')
    args=parser.parse_args()
    rom=ROM(args.rom)
    try:
        database=build(rom,args.cache)
        font=fonts(rom)
        args.out.mkdir(parents=True,exist_ok=True)
        for name,value in [('database.json',database),('game-font.json',font)]:
            (args.out/name).write_text(json.dumps(value,ensure_ascii=False,separators=(',',':')))
        if not args.no_sprites:
            folder=args.out/'sprites';folder.mkdir(exist_ok=True)
            def get_sprite(pair):
                i,shiny=pair
                prefix='shiny/' if shiny else ''
                url=f'https://raw.githubusercontent.com/PokeAPI/sprites/master/sprites/pokemon/versions/generation-v/black-white/{prefix}{i}.png'
                name=f'{i}{"-shiny" if shiny else ""}.png'
                download(url,folder/name)
            with ThreadPoolExecutor(max_workers=6) as pool:
                list(pool.map(get_sprite,((i,shiny) for i in range(1,650) for shiny in [False,True])))
        print(f'Resources prepared in {args.out}. No save files were read.')
    finally:
        rom.f.close()

if __name__=='__main__':
    main()
