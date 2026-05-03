"""Generate Slay the Spire knowledge base from spirespy raw data.

Builds per-character files (cards, relics, tier-grouped index) and shared
files (colorless cards, curses, global index lookup). Run with no args to
regenerate everything; pass a character name to regenerate just that one.

    python3 data/_generate.py                 # all characters + shared
    python3 data/_generate.py silent          # just silent
    python3 data/_generate.py ironclad silent # multiple
    python3 data/_generate.py shared          # just shared files
"""
import json
import sys
from pathlib import Path
from collections import defaultdict

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / 'data'

with open(DATA / '_raw_cards_all.json') as f: ALL_CARDS = json.load(f)
with open(DATA / '_raw_relics_all.json') as f: ALL_RELICS = json.load(f)

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
                          'character': r.get('character'),
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
# Helpers
# ---------------------------------------------------------------------------
def desc_clean(s):
    """Remove ^ markers used to highlight upgrade values; decode HTML entities."""
    if not s: return ''
    import html
    s = s.replace('&#46', '.')
    s = html.unescape(s)
    return s.replace('^', '')

def resolve_refs(idx_list, character):
    """Map index list to display names, filtering to the character's pickable scope.

    Pickable scope = same-class cards + colorless + curse + relics that the
    character can actually find (any + same-character).
    """
    out = []
    for i in idx_list or []:
        e = LOOKUP.get(i)
        if not e: continue
        if e['kind'] == 'card':
            if e.get('character') not in (character, 'colorless', 'curse'):
                continue
        else:  # relic
            if e.get('character') not in (character, 'any'):
                continue
        out.append(e['name'])
    return out

def fmt_mana_clean(c):
    m = c['cost']
    mp = c['cost_upgraded']
    if m is None: return '-'
    if mp is not None and mp != m:
        return f"{m}→{mp}"
    return str(m)

# ---------------------------------------------------------------------------
# Per-character build
# ---------------------------------------------------------------------------
def build_character(character):
    """Generate <char>_cards.md, <char>_relics.md, <char>_cards_index.md,
    <char>_relics_index.md, data/<char>_cards.json, data/<char>_relics.json."""
    char_cards = [c for c in ALL_CARDS if c.get('character') == character]
    char_relics = [r for r in ALL_RELICS
                   if r.get('character') in ('any', character)]
    tier_field = f'tier{character}'

    # -- Clean cards JSON ---------------------------------------------------
    clean_cards = []
    for c in char_cards:
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
            'super_synergies': resolve_refs(c.get('supersynergies'), character),
            'synergies': resolve_refs(c.get('synergies'), character),
            'antisynergies': resolve_refs(c.get('antisynergies'), character),
            'index': c.get('index'),
        })
    clean_cards.sort(key=lambda x: x['name'])

    # -- Clean relics JSON --------------------------------------------------
    clean_relics = []
    for r in char_relics:
        clean_relics.append({
            'name': r['name'],
            'rarity': r.get('rarity'),
            'character': r.get('character'),
            'description': desc_clean(r.get('description')),
            'tier': r.get(tier_field) or r.get('tier'),
            'tags': r.get('tags', []),
            'statement': r.get('statement', ''),
            'flavor': r.get('flavor', ''),
            'super_synergies': resolve_refs(r.get('supersynergies'), character),
            'synergies': resolve_refs(r.get('synergies'), character),
            'antisynergies': resolve_refs(r.get('antisynergies'), character),
            'index': r.get('index'),
        })
    clean_relics.sort(key=lambda x: x['name'])

    (DATA / f'{character}_cards.json').write_text(json.dumps(clean_cards, indent=2))
    (DATA / f'{character}_relics.json').write_text(json.dumps(clean_relics, indent=2))

    char_title = character.capitalize()

    # -- Cards markdown -----------------------------------------------------
    def card_block(c):
        cost = fmt_mana_clean(c)
        tier = c['tier'] or '?'
        head = f"### {c['name']} `{c['type']}` `{c['rarity']}` `cost {cost}` `tier {tier}`"
        lines = [head]
        if c['tags']:
            lines.append(f"_tags_: {', '.join(c['tags'])}")
        lines.append(f"**Effect.** {c['description']}")
        if c['description_upgraded'] and c['description_upgraded'] != c['description']:
            extra = (f" _(cost {c['cost_upgraded']})_"
                     if c['cost_upgraded'] is not None and c['cost_upgraded'] != c['cost']
                     else '')
            lines.append(f"**Upgrade.** {c['description_upgraded']}{extra}")
        if c['statement']:
            lines.append(f"**Notes.** {c['statement']}")
        if c['super_synergies']:
            lines.append(f"**★ Super-synergies.** {', '.join(c['super_synergies'])}")
        if c['synergies']:
            syn = c['synergies']
            if len(syn) > 25:
                lines.append(f"**Synergies ({len(syn)}).** {', '.join(syn[:25])}, …")
            else:
                lines.append(f"**Synergies.** {', '.join(syn)}")
        if c['antisynergies']:
            lines.append(f"**Anti-synergies.** {', '.join(c['antisynergies'])}")
        return '\n\n'.join(lines)

    by_tier = defaultdict(list)
    for c in clean_cards:
        by_tier[c['tier'] or '?'].append(c)

    cards_md = [f'# {char_title} — Cards',
                '',
                f'All {len(clean_cards)} unique {char_title} cards (excluding the universal '
                'Strike/Defend starters).',
                'Sourced from spirespy.maybelatergames.co.uk. Tier letters are the community',
                "rating; the **Notes** field is the curator's strategic commentary.",
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
    for t in sorted(by_tier.keys(), key=tier_key):
        cards_md.append(f"## Tier {t}")
        cards_md.append('')
        for c in sorted(by_tier[t], key=lambda x: x['name']):
            cards_md.append(card_block(c))
            cards_md.append('')
    (ROOT / f'{character}_cards.md').write_text('\n'.join(cards_md))

    # -- Cards lean index ---------------------------------------------------
    idx_lines = [f'# {char_title} — Cards (lean index)',
                 '',
                 f'One line per card: name, type, cost, tier, one-line effect.',
                 f'Total: {len(clean_cards)} cards. Use this as a baseline scan; drill into',
                 f'`{character}_cards.md` for Notes, synergies, and upgrade text.',
                 '']
    for t in sorted(by_tier.keys(), key=tier_key):
        idx_lines.append(f"## Tier {t}")
        idx_lines.append('')
        for c in sorted(by_tier[t], key=lambda x: x['name']):
            cost = fmt_mana_clean(c)
            # Truncate effect to one line; prefer base effect.
            effect = (c['description'] or '').replace('\n', ' ').strip()
            if len(effect) > 110:
                effect = effect[:107] + '...'
            idx_lines.append(f"- **{c['name']}** `{c['type']}` `cost {cost}` — {effect}")
        idx_lines.append('')
    (ROOT / f'{character}_cards_index.md').write_text('\n'.join(idx_lines))

    # -- Tag reverse-index (cards by tag) -----------------------------------
    # "All poison cards" / "all discard cards" / etc. — the spirespy `tags`
    # field maps directly to archetype categories.
    by_tag = defaultdict(list)
    for c in clean_cards:
        for tag in c.get('tags', []):
            by_tag[tag].append(c)
    tag_lines = [f'# {char_title} — Cards by tag',
                 '',
                 f'Reverse index of the `tags` field on each card. Use this to find every',
                 f'card in an archetype quickly (e.g. "all poison cards").',
                 f'Total tags: {len(by_tag)}. Cards may appear under multiple tags.',
                 '']
    for tag in sorted(by_tag.keys()):
        items = sorted(by_tag[tag], key=lambda x: (tier_key(x['tier']), x['name']))
        tag_lines.append(f"## `{tag}` ({len(items)})")
        tag_lines.append('')
        for c in items:
            cost = fmt_mana_clean(c)
            tier = c['tier'] or '?'
            effect = (c['description'] or '').replace('\n', ' ').strip()
            if len(effect) > 90:
                effect = effect[:87] + '...'
            tag_lines.append(f"- **{c['name']}** `{c['type']}` `cost {cost}` `tier {tier}` — {effect}")
        tag_lines.append('')
    (ROOT / f'{character}_cards_by_tag.md').write_text('\n'.join(tag_lines))

    # -- Relics markdown ----------------------------------------------------
    def relic_block(r):
        tier = r['tier'] or '?'
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
        relics_by_tier[r['tier'] or '?'].append(r)

    relics_md = [f'# {char_title} — Relics',
                 '',
                 f'{len(clean_relics)} relics that the {char_title} can encounter '
                 f'(`character: any` or `character: {character}`).',
                 f'Tier rating is the {char_title}-specific rating from spirespy.',
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
        line = ', '.join(f"{r['name']} ({r['tier'] or '?'})" for r in items)
        relics_md.append(f"- **{rar}** ({len(items)}): {line}")
    relics_md.append('')
    for t in sorted(relics_by_tier.keys(), key=tier_key):
        relics_md.append(f"## Tier {t}")
        relics_md.append('')
        for r in sorted(relics_by_tier[t], key=lambda x: x['name']):
            relics_md.append(relic_block(r))
            relics_md.append('')
    (ROOT / f'{character}_relics.md').write_text('\n'.join(relics_md))

    # -- Relics lean index --------------------------------------------------
    idx_lines = [f'# {char_title} — Relics (lean index)',
                 '',
                 f'One line per relic: name, rarity, tier, one-line effect.',
                 f'Total: {len(clean_relics)}. Drill into `{character}_relics.md` for Notes.',
                 '']
    for t in sorted(relics_by_tier.keys(), key=tier_key):
        idx_lines.append(f"## Tier {t}")
        idx_lines.append('')
        for r in sorted(relics_by_tier[t], key=lambda x: x['name']):
            effect = (r['description'] or '').replace('\n', ' ').strip()
            if len(effect) > 110:
                effect = effect[:107] + '...'
            idx_lines.append(f"- **{r['name']}** `{r['rarity']}` — {effect}")
        idx_lines.append('')
    (ROOT / f'{character}_relics_index.md').write_text('\n'.join(idx_lines))

    # -- Relics by tag ------------------------------------------------------
    by_tag_r = defaultdict(list)
    for r in clean_relics:
        for tag in r.get('tags', []):
            by_tag_r[tag].append(r)
    rtag_lines = [f'# {char_title} — Relics by tag',
                  '',
                  f'Reverse index of the `tags` field on each relic. Useful for "what relics',
                  f'support a poison deck?" or "what gives me block?". {len(by_tag_r)} tags.',
                  '']
    for tag in sorted(by_tag_r.keys()):
        items = sorted(by_tag_r[tag], key=lambda x: (tier_key(x['tier']), x['name']))
        rtag_lines.append(f"## `{tag}` ({len(items)})")
        rtag_lines.append('')
        for r in items:
            tier = r['tier'] or '?'
            effect = (r['description'] or '').replace('\n', ' ').strip()
            if len(effect) > 90:
                effect = effect[:87] + '...'
            rtag_lines.append(f"- **{r['name']}** `{r['rarity']}` `tier {tier}` — {effect}")
        rtag_lines.append('')
    (ROOT / f'{character}_relics_by_tag.md').write_text('\n'.join(rtag_lines))

    print(f'  built {character}: {len(clean_cards)} cards, {len(clean_relics)} relics')


# ---------------------------------------------------------------------------
# Shared files: colorless.md, curses.md, JSONs, index_lookup
# ---------------------------------------------------------------------------
def build_shared():
    COLORLESS_CARDS = [c for c in ALL_CARDS if c.get('character') == 'colorless']
    CURSE_CARDS = [c for c in ALL_CARDS if c.get('character') == 'curse']

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

    (DATA / 'colorless_cards.json').write_text(json.dumps(clean_colorless, indent=2))
    (DATA / 'curses.json').write_text(json.dumps(clean_curses, indent=2))
    (DATA / 'index_lookup.json').write_text(json.dumps(LOOKUP, indent=2))

    # colorless.md
    col_by_tier = defaultdict(list)
    for c in clean_colorless:
        col_by_tier[c['tier'] or '?'].append(c)
    col_md = ['# Colorless cards',
              '',
              f'{len(clean_colorless)} colorless cards. Available from events, Prismatic Shard, '
              'Bottled Lightning shenanigans, etc. Pickable by every character.',
              '',
              '## Index by tier',
              '']
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

    # curses.md
    cur_md = ['# Curses', '',
              f'{len(clean_curses)} curse cards. You generally do not want them, but knowing the',
              'cost helps decide whether to take a Curse-related event or relic '
              '(Du-Vu Doll, Blue Candle, etc.).',
              '']
    for c in sorted(clean_curses, key=lambda x: x['name']):
        cur_md.append(f"### {c['name']}")
        cur_md.append(f"**Effect.** {c['description']}")
        if c['statement']: cur_md.append(f"**Notes.** {c['statement']}")
        cur_md.append('')
    (ROOT / 'curses.md').write_text('\n\n'.join(cur_md))

    print(f'  built shared: {len(clean_colorless)} colorless, {len(clean_curses)} curses, '
          f'{len(LOOKUP)} index entries')


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
KNOWN = ['ironclad', 'silent', 'defect', 'watcher']

if __name__ == '__main__':
    args = sys.argv[1:]
    if not args:
        targets = KNOWN + ['shared']
    else:
        targets = args

    print('generating:')
    for t in targets:
        if t == 'shared':
            build_shared()
        elif t in KNOWN:
            build_character(t)
        else:
            print(f'  unknown target: {t} (expected one of {KNOWN + ["shared"]})')

    print('\nfiles:')
    for p in sorted(ROOT.glob('*.md')) + sorted(DATA.glob('*.json')):
        print(f'  {p.relative_to(ROOT)} ({p.stat().st_size} bytes)')
