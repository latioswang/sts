"""Generate Slay the Spire Ironclad knowledge base from spirespy data."""
import json
from pathlib import Path
from collections import defaultdict

ROOT = Path('/home/user/sts')
DATA = ROOT / 'data'
DATA.mkdir(exist_ok=True)

with open('/tmp/cards.json') as f: ALL_CARDS = json.load(f)
with open('/tmp/relics.json') as f: ALL_RELICS = json.load(f)

# ---------------------------------------------------------------------------
# Index → name lookup (resolves synergy references)
# ---------------------------------------------------------------------------
LOOKUP = {}
for c in ALL_CARDS:
    LOOKUP[c['index']] = {'kind': 'card', 'name': c['name'],
                          'character': c.get('character'),
                          'type': c.get('type'),
                          'rarity': c.get('rarity')}
for r in ALL_RELICS:
    LOOKUP[r['index']] = {'kind': 'relic', 'name': r['name'],
                          'rarity': r.get('rarity')}

# ---------------------------------------------------------------------------
# Tier ranking: S+ > S > S- > A+ > ... > E-
# ---------------------------------------------------------------------------
TIER_ORDER = []
for letter in ['S', 'A', 'B', 'C', 'D', 'E', 'F']:
    for suffix in ['+', '', '-']:
        TIER_ORDER.append(letter + suffix)
TIER_RANK = {t: i for i, t in enumerate(TIER_ORDER)}

def tier_key(t):
    return TIER_RANK.get(t or '', 999)

# ---------------------------------------------------------------------------
# Filter Ironclad-relevant cards & relics
# ---------------------------------------------------------------------------
IRONCLAD_CARDS = [c for c in ALL_CARDS if c.get('character') == 'ironclad']
COLORLESS_CARDS = [c for c in ALL_CARDS if c.get('character') == 'colorless']
CURSE_CARDS = [c for c in ALL_CARDS if c.get('character') == 'curse']

# Relics: all "any" + ironclad-specific
IRONCLAD_RELICS = [r for r in ALL_RELICS
                   if r.get('character') in ('any', 'ironclad')]

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def desc_clean(s):
    """Remove ^ markers used to highlight upgrade values; decode HTML entities."""
    if not s: return ''
    import html
    # spirespy data sometimes has '&#46' (missing trailing ';'). Patch first.
    s = s.replace('&#46', '.')
    s = html.unescape(s)
    return s.replace('^', '')

def resolve_refs(idx_list, only_ironclad=True):
    """Map index list to display names, optionally filtering to Ironclad scope."""
    out = []
    for i in idx_list or []:
        e = LOOKUP.get(i)
        if not e: continue
        if only_ironclad and e['kind'] == 'card':
            if e.get('character') not in ('ironclad', 'colorless', 'curse', 'any'):
                continue
        out.append(e['name'])
    return out

def fmt_mana(c):
    m = c.get('mana')
    mp = c.get('manaplus')
    if m is None: return '-'
    if mp is not None and mp != m:
        return f"{m} ({mp}+)"
    return str(m)

# ---------------------------------------------------------------------------
# Build clean Ironclad-cards JSON (with resolved synergy names)
# ---------------------------------------------------------------------------
clean_cards = []
for c in IRONCLAD_CARDS:
    clean_cards.append({
        'name': c['name'],
        'type': c.get('type'),
        'rarity': c.get('rarity'),
        'cost': c.get('mana'),
        'cost_upgraded': c.get('manaplus'),
        'description': desc_clean(c.get('description')),
        'description_upgraded': desc_clean(c.get('descriptionplus')),
        'tier': c.get('tier'),
        'tags': c.get('tags', []),
        'statement': c.get('statement', ''),
        'super_synergies': resolve_refs(c.get('supersynergies')),
        'synergies': resolve_refs(c.get('synergies')),
        'antisynergies': resolve_refs(c.get('antisynergies')),
        'index': c.get('index'),
    })
clean_cards.sort(key=lambda x: x['name'])

clean_relics = []
for r in IRONCLAD_RELICS:
    clean_relics.append({
        'name': r['name'],
        'rarity': r.get('rarity'),
        'character': r.get('character'),
        'description': desc_clean(r.get('description')),
        'tier_ironclad': r.get('tierironclad') or r.get('tier'),
        'tags': r.get('tags', []),
        'statement': r.get('statement', ''),
        'flavor': r.get('flavor', ''),
        'super_synergies': resolve_refs(r.get('supersynergies')),
        'synergies': resolve_refs(r.get('synergies')),
        'antisynergies': resolve_refs(r.get('antisynergies')),
        'index': r.get('index'),
    })
clean_relics.sort(key=lambda x: x['name'])

clean_colorless = []
for c in COLORLESS_CARDS:
    clean_colorless.append({
        'name': c['name'], 'type': c.get('type'), 'rarity': c.get('rarity'),
        'cost': c.get('mana'), 'cost_upgraded': c.get('manaplus'),
        'description': desc_clean(c.get('description')),
        'description_upgraded': desc_clean(c.get('descriptionplus')),
        'tier': c.get('tier'), 'tags': c.get('tags', []),
        'statement': c.get('statement', ''),
        'index': c.get('index'),
    })
clean_colorless.sort(key=lambda x: x['name'])

clean_curses = []
for c in CURSE_CARDS:
    clean_curses.append({
        'name': c['name'], 'description': desc_clean(c.get('description')),
        'tags': c.get('tags', []), 'tier': c.get('tier'),
        'statement': c.get('statement', ''), 'index': c.get('index'),
    })
clean_curses.sort(key=lambda x: x['name'])

# Save JSON
(DATA / 'ironclad_cards.json').write_text(json.dumps(clean_cards, indent=2))
(DATA / 'ironclad_relics.json').write_text(json.dumps(clean_relics, indent=2))
(DATA / 'colorless_cards.json').write_text(json.dumps(clean_colorless, indent=2))
(DATA / 'curses.json').write_text(json.dumps(clean_curses, indent=2))
(DATA / 'index_lookup.json').write_text(json.dumps(LOOKUP, indent=2))

# ---------------------------------------------------------------------------
# Markdown: cards.md (Ironclad cards by tier, with statement + synergies)
# ---------------------------------------------------------------------------
def card_block(c):
    cost = fmt_mana_clean(c)
    tier = c['tier'] or '?'
    head = f"### {c['name']} `{c['type']}` `{c['rarity']}` `cost {cost}` `tier {tier}`"
    lines = [head]
    if c['tags']:
        lines.append(f"_tags_: {', '.join(c['tags'])}")
    lines.append(f"**Effect.** {c['description']}")
    if c['description_upgraded'] and c['description_upgraded'] != c['description']:
        lines.append(f"**Upgrade.** {c['description_upgraded']}"
                     + (f" _(cost {c['cost_upgraded']})_"
                        if c['cost_upgraded'] is not None and c['cost_upgraded'] != c['cost']
                        else ''))
    if c['statement']:
        lines.append(f"**Notes.** {c['statement']}")
    if c['super_synergies']:
        lines.append(f"**★ Super-synergies.** {', '.join(c['super_synergies'])}")
    if c['synergies']:
        syn = c['synergies']
        if len(syn) > 25:
            shown = ', '.join(syn[:25])
            lines.append(f"**Synergies ({len(syn)}).** {shown}, …")
        else:
            lines.append(f"**Synergies.** {', '.join(syn)}")
    if c['antisynergies']:
        lines.append(f"**Anti-synergies.** {', '.join(c['antisynergies'])}")
    return '\n\n'.join(lines)

def fmt_mana_clean(c):
    m = c['cost']
    mp = c['cost_upgraded']
    if m is None: return '-'
    if mp is not None and mp != m:
        return f"{m}→{mp}"
    return str(m)

# Group by tier
by_tier = defaultdict(list)
for c in clean_cards:
    by_tier[c['tier'] or '?'].append(c)

cards_md = ['# Ironclad — Cards',
            '',
            'All 73 unique Ironclad cards (excluding the universal Strike/Defend starters).',
            'Sourced from spirespy.maybelatergames.co.uk. Tier letters are the community',
            'rating; the **Notes** field is the curator\'s strategic commentary.',
            '',
            '> Quick reading: tier ≥ B is generally pickable; A/S are auto-picks.',
            '> Watch for `★ Super-synergies` — those are the strongest pairs.',
            '',
            '## Index by tier',
            '']

for t in sorted(by_tier.keys(), key=tier_key):
    names = sorted(c['name'] for c in by_tier[t])
    cards_md.append(f"- **{t}** ({len(names)}): {', '.join(names)}")
cards_md.append('')

# Tier sections
for t in sorted(by_tier.keys(), key=tier_key):
    cards_md.append(f"## Tier {t}")
    cards_md.append('')
    for c in sorted(by_tier[t], key=lambda x: x['name']):
        cards_md.append(card_block(c))
        cards_md.append('')

(ROOT / 'cards.md').write_text('\n'.join(cards_md))

# ---------------------------------------------------------------------------
# Markdown: relics.md (by ironclad-tier, then rarity)
# ---------------------------------------------------------------------------
def relic_block(r):
    tier = r['tier_ironclad'] or '?'
    head = f"### {r['name']} `{r['rarity']}` `tier {tier}`"
    lines = [head]
    if r['tags']:
        lines.append(f"_tags_: {', '.join(r['tags'])}")
    lines.append(f"**Effect.** {r['description']}")
    if r['statement']:
        lines.append(f"**Notes.** {r['statement']}")
    if r['super_synergies']:
        lines.append(f"**★ Super-synergies.** {', '.join(r['super_synergies'])}")
    if r['synergies']:
        syn = r['synergies']
        if len(syn) > 20:
            lines.append(f"**Synergies ({len(syn)}).** {', '.join(syn[:20])}, …")
        else:
            lines.append(f"**Synergies.** {', '.join(syn)}")
    if r['antisynergies']:
        lines.append(f"**Anti-synergies.** {', '.join(r['antisynergies'])}")
    return '\n\n'.join(lines)

relics_by_tier = defaultdict(list)
for r in clean_relics:
    relics_by_tier[r['tier_ironclad'] or '?'].append(r)

relics_md = ['# Ironclad — Relics',
            '',
            f'{len(clean_relics)} relics that the Ironclad can encounter '
            '(`character: any` or `character: ironclad`).',
            'Tier rating is the Ironclad-specific rating from spirespy.',
            '',
            '## Index by tier',
            '']
for t in sorted(relics_by_tier.keys(), key=tier_key):
    names = sorted(r['name'] for r in relics_by_tier[t])
    relics_md.append(f"- **{t}** ({len(names)}): {', '.join(names)}")
relics_md.append('')
relics_md.append('## Index by rarity (alphabetical)')
relics_md.append('')
relics_by_rarity = defaultdict(list)
for r in clean_relics:
    relics_by_rarity[r['rarity'] or '?'].append(r)
for rar in ['starter', 'common', 'uncommon', 'rare', 'shop', 'event', 'boss']:
    if rar not in relics_by_rarity: continue
    items = sorted(relics_by_rarity[rar], key=lambda x: x['name'])
    line = ', '.join(f"{r['name']} ({r['tier_ironclad'] or '?'})" for r in items)
    relics_md.append(f"- **{rar}** ({len(items)}): {line}")
relics_md.append('')

for t in sorted(relics_by_tier.keys(), key=tier_key):
    relics_md.append(f"## Tier {t}")
    relics_md.append('')
    for r in sorted(relics_by_tier[t], key=lambda x: x['name']):
        relics_md.append(relic_block(r))
        relics_md.append('')

(ROOT / 'relics.md').write_text('\n'.join(relics_md))

# ---------------------------------------------------------------------------
# Markdown: colorless.md (cards Ironclad can pick up via events / Prismatic Shard)
# ---------------------------------------------------------------------------
col_md = ['# Colorless cards (pickable by Ironclad)',
          '',
          f'{len(clean_colorless)} colorless cards. Available from events, Prismatic Shard, '
          'Bottled Lightning shenanigans, etc.',
          '',
          '## Index by tier',
          '']
col_by_tier = defaultdict(list)
for c in clean_colorless: col_by_tier[c['tier'] or '?'].append(c)
for t in sorted(col_by_tier.keys(), key=tier_key):
    names = sorted(c['name'] for c in col_by_tier[t])
    col_md.append(f"- **{t}** ({len(names)}): {', '.join(names)}")
col_md.append('')
for t in sorted(col_by_tier.keys(), key=tier_key):
    col_md.append(f"## Tier {t}")
    col_md.append('')
    for c in sorted(col_by_tier[t], key=lambda x: x['name']):
        cost = fmt_mana_clean(c)
        col_md.append(f"### {c['name']} `{c['type']}` `{c['rarity']}` `cost {cost}` `tier {t}`")
        if c['tags']: col_md.append(f"_tags_: {', '.join(c['tags'])}")
        col_md.append(f"**Effect.** {c['description']}")
        if c['description_upgraded'] and c['description_upgraded'] != c['description']:
            extra = (f" _(cost {c['cost_upgraded']})_"
                     if c['cost_upgraded'] is not None and c['cost_upgraded'] != c['cost']
                     else '')
            col_md.append(f"**Upgrade.** {c['description_upgraded']}{extra}")
        if c['statement']: col_md.append(f"**Notes.** {c['statement']}")
        col_md.append('')
(ROOT / 'colorless.md').write_text('\n\n'.join(col_md))

# ---------------------------------------------------------------------------
# Markdown: curses.md
# ---------------------------------------------------------------------------
cur_md = ['# Curses', '',
          f'{len(clean_curses)} curse cards. You generally do not want them, but knowing the',
          'cost helps decide whether to take a Curse-related event or relic (Du-Vu Doll, etc.).',
          '']
for c in sorted(clean_curses, key=lambda x: x['name']):
    cur_md.append(f"### {c['name']}")
    cur_md.append(f"**Effect.** {c['description']}")
    if c['statement']: cur_md.append(f"**Notes.** {c['statement']}")
    cur_md.append('')
(ROOT / 'curses.md').write_text('\n\n'.join(cur_md))

print('done. wrote:')
for p in sorted(ROOT.glob('*.md')) + sorted(DATA.glob('*.json')):
    print(' ', p, p.stat().st_size, 'bytes')
