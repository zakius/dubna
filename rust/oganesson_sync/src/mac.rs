// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

//! Multi-Account Containers' `storage.sync` area (Gecko ID
//! `@testpilot-containers`), as the browser's containers and Traffic Control
//! rules. The add-on's format, from its `src/js/background/sync.js` and
//! `assignManager.js`:
//!
//! - `identity@@_<uuid>`: a container, `{name, icon, color, cookieStoreId,
//!   macAddonUUID}`. The uuid (`macAddonUUID`) is the same on every device;
//!   `cookieStoreId` is the writer's own and means nothing elsewhere. Colors
//!   use Firefox's names before 153 (`turquoise`, `toolbar`).
//! - `siteContainerMap@@_<host>`: a site assignment, `{userContextId, neverAsk,
//!   identityMacAddonUUID, hostname}`: pages of exactly `host` open in the
//!   container. A site on another port has the port glued to the host
//!   (`localhost8080`), in the key and in `hostname`.
//! - `deletedIdentityList` and `deletedSiteList`: the uuids and site keys a
//!   device deleted, for the others to delete too.
//! - `MACinstance:<uuid>`: per install, `{timestamp, identities,
//!   siteAssignments}`, what it has. The add-on prunes a deletion from the
//!   lists once no install lists the item, and an install after 30 days.
//!
//! A container here is identified by its local id, which differs between
//! devices, and the engine state pairs it with a uuid. A site assignment is a
//! Traffic Control rule that is enabled, has the single pattern `.<host>` or
//! `.<host>:<port>` (that host only, as the add-on matches) and opens a
//! container of the list. Other rules (more patterns, subdomains, paths, a temporary container
//! or none, switched off) stay local, as do temporary containers, and the
//! containers' order.
//!
//! Each sync, `stage` writes what changed here since the state the browser
//! last saved (`baseline`) into the store, before the store merges the
//! server's copy; `finish` turns the merged area into ops for the browser.
//! Comparing with the baseline, not with the store, keeps a remote change the
//! browser never applied (it quit first) from looking like a local edit to
//! upload. When both sides changed one key, the server's wins, as in the
//! add-on.

use std::collections::{BTreeMap, BTreeSet, HashSet};

use serde::{Deserialize, Serialize};
use serde_json::{json, Map, Value};
use webext_storage::{SYNC_MAX_ITEMS, SYNC_QUOTA_BYTES, SYNC_QUOTA_BYTES_PER_ITEM};

pub const EXT_ID: &str = "@testpilot-containers";

const IDENTITY: &str = "identity@@_";
const SITE: &str = "siteContainerMap@@_";
const INSTANCE: &str = "MACinstance:";
const DELETED_IDENTITIES: &str = "deletedIdentityList";
const DELETED_SITES: &str = "deletedSiteList";
/// Our install's key is rewritten at least this often, so the add-on doesn't
/// prune it (it does after 30 days).
const INSTANCE_REFRESH_MS: i64 = 24 * 60 * 60 * 1000;

/// The browser's snapshot: its `containers.list` and
/// `containers.traffic_control` prefs, as they are.
#[derive(Deserialize, Default)]
pub struct Local {
    #[serde(default)]
    pub containers: Vec<Container>,
    #[serde(default)]
    pub rules: Vec<Rule>,
}

#[derive(Serialize, Deserialize, Clone, Debug, Default, PartialEq)]
pub struct Container {
    pub id: String,
    #[serde(default)]
    pub name: String,
    #[serde(default)]
    pub icon: String,
    #[serde(default)]
    pub color: String,
}

#[derive(Deserialize, Clone, Debug)]
pub struct Rule {
    #[serde(default)]
    pub url_filter: String,
    #[serde(default)]
    pub container: String,
    #[serde(default = "yes")]
    pub enabled: bool,
}

fn yes() -> bool {
    true
}

/// A container as both sides see it.
#[derive(Serialize, Deserialize, Clone, Debug, Default, PartialEq)]
pub struct Look {
    pub name: String,
    pub icon: String,
    pub color: String,
}

impl Look {
    fn of(c: &Container) -> Self {
        Look { name: c.name.clone(), icon: c.icon.clone(), color: legacy_color(&c.color).to_string() }
    }
}

/// What syncs: containers by uuid, and sites (hosts) by the uuid of their
/// container.
#[derive(Serialize, Deserialize, Clone, Debug, Default, PartialEq)]
pub struct View {
    pub identities: BTreeMap<String, Look>,
    pub sites: BTreeMap<String, String>,
}

/// The engine state the browser saves once it applied the ops.
#[derive(Serialize, Deserialize, Clone, Debug, Default)]
pub struct State {
    /// The uuid of each container of the list, by local id.
    #[serde(default)]
    pub uuids: BTreeMap<String, String>,
    /// What the browser had of the area when it saved this state.
    #[serde(default)]
    pub baseline: View,
    /// Deleted here: uuids and hosts kept out of the area until the deletion
    /// lists have them, or no other install has them any more.
    #[serde(default)]
    pub deleted_identities: BTreeSet<String>,
    #[serde(default)]
    pub deleted_sites: BTreeSet<String>,
    /// This install's uuid, for its `MACinstance:` key.
    #[serde(default)]
    pub instance: String,
}

/// A change for the browser to make.
#[derive(Serialize, Debug, PartialEq)]
#[serde(tag = "op", rename_all = "snake_case")]
pub enum Op {
    /// Adds the container, or sets its name, icon and color.
    Container { id: String, name: String, icon: String, color: String },
    DeleteContainer { id: String },
    /// Adds the rule `.<host>` for the container, or moves the host's rule
    /// to it.
    Site { host: String, container: String },
    /// Removes the host's rules.
    DeleteSite { host: String },
}

fn identity_key(uuid: &str) -> String {
    format!("{IDENTITY}{uuid}")
}

/// The key of a site, `host` or `host:port`, with the port glued on as the
/// add-on does.
fn site_key(site: &str) -> String {
    format!("{SITE}{}", site.replace(':', ""))
}

/// Firefox 153's color names, as the add-on stores them.
fn legacy_color(color: &str) -> &str {
    match color {
        "cyan" => "turquoise",
        "gray" => "toolbar",
        other => other,
    }
}

fn text(value: &Value, field: &str) -> String {
    value[field].as_str().unwrap_or_default().to_string()
}

/// A host the add-on's key can name and a rule's `.<host>` pattern matches
/// alone: lowercase labels of letters, digits and hyphens. One that ends in
/// a digit must be an IPv4 address, as the add-on glues a port to the host.
fn valid_host(host: &str) -> bool {
    let labels: Vec<&str> = host.split('.').collect();
    let label_ok = |l: &&str| {
        !l.is_empty()
            && l.len() <= 63
            && !l.starts_with('-')
            && !l.ends_with('-')
            && l.bytes().all(|b| b.is_ascii_lowercase() || b.is_ascii_digit() || b == b'-')
    };
    if host.len() > 253 || !labels.iter().all(label_ok) {
        return false;
    }
    if host.ends_with(|c: char| c.is_ascii_digit()) {
        return labels.len() == 4
            && labels.iter().all(|l| l.parse::<u8>().is_ok_and(|n| n.to_string() == *l));
    }
    true
}

/// A site the add-on's key can name: a valid host, or one with a port other
/// than 80 and 443, which the add-on leaves out. An IPv4 address with a port
/// isn't one: glued together, they can't be told apart again.
fn valid_site(site: &str) -> bool {
    let Some((host, port)) = site.split_once(':') else { return valid_host(site) };
    let port_ok = !port.starts_with('0')
        && port.parse::<u16>().is_ok_and(|n| n != 80 && n != 443 && n.to_string() == port);
    port_ok && valid_host(host) && !host.ends_with(|c: char| c.is_ascii_digit())
}

/// The site a key names: a host, or one followed by digits, which are its
/// port (a host other than an IPv4 address ends in a letter).
fn key_site(suffix: &str) -> Option<String> {
    if valid_host(suffix) {
        return Some(suffix.to_string());
    }
    let host = suffix.trim_end_matches(|c: char| c.is_ascii_digit());
    let site = format!("{host}:{}", &suffix[host.len()..]);
    valid_site(&site).then_some(site)
}

/// The site of a rule that is one site exactly: the pattern `.<host>` or
/// `.<host>:<port>`.
fn rule_host(url_filter: &str) -> Option<&str> {
    url_filter.trim().strip_prefix('.').filter(|h| valid_site(h))
}

/// The add-on's area, read.
struct Area<'a>(&'a Map<String, Value>);

impl Area<'_> {
    /// Containers by uuid.
    fn identities(&self) -> BTreeMap<String, Look> {
        let mut out = BTreeMap::new();
        for (key, value) in self.0 {
            let Some(suffix) = key.strip_prefix(IDENTITY) else { continue };
            if !value.is_object() {
                continue;
            }
            let uuid = value["macAddonUUID"].as_str().unwrap_or(suffix);
            if uuid != suffix {
                continue; // Not the add-on's own shape.
            }
            let look = Look {
                name: text(value, "name"),
                icon: text(value, "icon"),
                color: legacy_color(&text(value, "color")).to_string(),
            };
            out.insert(uuid.to_string(), look);
        }
        out
    }

    /// Sites by host, with their container's uuid.
    fn sites(&self) -> BTreeMap<String, String> {
        let mut out = BTreeMap::new();
        for (key, value) in self.0 {
            let Some(suffix) = key.strip_prefix(SITE) else { continue };
            let Some(uuid) = value["identityMacAddonUUID"].as_str() else { continue };
            if let Some(site) = key_site(suffix) {
                out.insert(site, uuid.to_string());
            }
        }
        out
    }

    fn list(&self, key: &str) -> Vec<String> {
        let items = self.0.get(key).and_then(Value::as_array);
        items.into_iter().flatten().filter_map(|v| v.as_str().map(String::from)).collect()
    }

    /// Whether an install other than `ours` lists `item` under `field`.
    fn listed_elsewhere(&self, ours: &str, field: &str, item: &str) -> bool {
        self.0.iter().any(|(key, value)| {
            key.starts_with(INSTANCE)
                && key != ours
                && value[field].as_array().is_some_and(|a| a.iter().any(|v| v == item))
        })
    }
}

/// The browser's side of the area, with `uuids`.
fn local_view(local: &Local, uuids: &BTreeMap<String, String>) -> View {
    let mut view = View::default();
    for c in &local.containers {
        if let Some(uuid) = uuids.get(&c.id) {
            view.identities.insert(uuid.clone(), Look::of(c));
        }
    }
    for rule in &local.rules {
        let Some(host) = rule_host(&rule.url_filter) else { continue };
        let Some(uuid) = uuids.get(&rule.container) else { continue };
        if rule.enabled && local.containers.iter().any(|c| c.id == rule.container) {
            view.sites.entry(host.to_string()).or_insert_with(|| uuid.clone());
        }
    }
    view
}

/// A uuid in the add-on's form (it uses uuidv4).
fn is_uuid(id: &str) -> bool {
    id.len() == 36
        && id.bytes().enumerate().all(|(i, b)| {
            if matches!(i, 8 | 13 | 18 | 23) { b == b'-' } else { b.is_ascii_hexdigit() }
        })
}

fn new_uuid() -> String {
    let mut b = [0u8; 16];
    rc_crypto::rand::fill(&mut b).expect("random bytes");
    b[6] = (b[6] & 0x0f) | 0x40;
    b[8] = (b[8] & 0x3f) | 0x80;
    let h: String = b.iter().map(|x| format!("{x:02x}")).collect();
    format!("{}-{}-{}-{}-{}", &h[0..8], &h[8..12], &h[12..16], &h[16..20], &h[20..32])
}

/// Firefox's English names of the default containers, by their ids here,
/// where the browser names them in the user's language.
const DEFAULT_NAMES: [(&str, &str); 4] =
    [("personal", "Personal"), ("work", "Work"), ("banking", "Banking"), ("shopping", "Shopping")];

/// Gives each container of the list a uuid: a container of the area with the
/// same name in any case that has no container here yet, then, for a default
/// container, one with Firefox's English name for it, then its own id if that
/// is a uuid, else a new one.
fn pair(local: &Local, state: &mut State, area: &Map<String, Value>, deleted: &HashSet<String>) {
    state.uuids.retain(|id, _| local.containers.iter().any(|c| &c.id == id));
    let identities = Area(area).identities();
    for c in &local.containers {
        if state.uuids.contains_key(&c.id) {
            continue;
        }
        let taken: HashSet<&String> = state.uuids.values().collect();
        let free = |uuid: &String| {
            !taken.contains(uuid) && !deleted.contains(uuid) && !state.deleted_identities.contains(uuid)
        };
        let named = |name: &str| {
            let name = name.to_lowercase();
            identities.iter().find(|(uuid, look)| look.name.to_lowercase() == name && free(uuid))
        };
        let english = DEFAULT_NAMES.iter().find(|(id, _)| c.id == *id).map(|(_, name)| *name);
        let by_name = named(&c.name).or_else(|| english.and_then(named));
        let uuid = match by_name {
            Some((uuid, _)) => uuid.clone(),
            None if is_uuid(&c.id) && free(&c.id) => c.id.clone(),
            None => new_uuid(),
        };
        state.uuids.insert(c.id.clone(), uuid);
    }
}

/// The bytes an item adds to the area's JSON, which is what storage.sync's
/// quota counts: the key quoted, a colon, the value and a comma.
fn item_bytes(key: &str, value: &Value) -> usize {
    Value::from(key).to_string().len() + 1 + value.to_string().len() + 1
}

/// The keys to set and remove in the store before the merge, and how many
/// site assignments stay local because the area would go over storage.sync's
/// quota. `current` is the store's area, `incoming` the server's if this sync
/// downloaded it.
pub fn stage(
    local: &Local,
    state: &mut State,
    current: &Map<String, Value>,
    incoming: Option<&Map<String, Value>>,
    now_ms: i64,
) -> (Map<String, Value>, Vec<String>, usize) {
    // What the area will hold: the server's keys over the store's.
    let mut seen = current.clone();
    if let Some(incoming) = incoming {
        seen.extend(incoming.iter().map(|(k, v)| (k.clone(), v.clone())));
    }
    let mut deleted_ids: Vec<String> = Area(current).list(DELETED_IDENTITIES);
    let mut deleted_sites: Vec<String> = Area(current).list(DELETED_SITES);
    for (list, key) in [(&mut deleted_ids, DELETED_IDENTITIES), (&mut deleted_sites, DELETED_SITES)] {
        for item in incoming.map(|i| Area(i).list(key)).unwrap_or_default() {
            if !list.contains(&item) {
                list.push(item);
            }
        }
    }
    let ids_before = deleted_ids.clone();
    let sites_before = deleted_sites.clone();
    let deleted: HashSet<String> = deleted_ids.iter().cloned().collect();

    pair(local, state, &seen, &deleted);
    let view = local_view(local, &state.uuids);
    let mut set = Map::new();
    let mut remove = vec![];

    for (uuid, look) in &view.identities {
        // A deletion elsewhere wins over an edit here.
        if deleted.contains(uuid) {
            continue;
        }
        let key = identity_key(uuid);
        let changed = state.baseline.identities.get(uuid) != Some(look);
        if !changed && seen.contains_key(&key) {
            continue;
        }
        let mut value = seen.get(&key).and_then(Value::as_object).cloned().unwrap_or_default();
        let id = state.uuids.iter().find(|(_, u)| *u == uuid).map(|(id, _)| id.clone());
        value.insert("name".into(), json!(look.name));
        value.insert("icon".into(), json!(look.icon));
        value.insert("color".into(), json!(look.color));
        value.insert("macAddonUUID".into(), json!(uuid));
        value
            .entry("cookieStoreId")
            .or_insert_with(|| json!(format!("firefox-container-{}", id.unwrap_or_default())));
        set.insert(key, Value::Object(value));
    }
    for uuid in state.baseline.identities.keys() {
        if !view.identities.contains_key(uuid) {
            state.deleted_identities.insert(uuid.clone());
        }
    }
    for uuid in &state.deleted_identities {
        let key = identity_key(uuid);
        if seen.contains_key(&key) {
            remove.push(key);
        }
        if !deleted_ids.contains(uuid) {
            deleted_ids.push(uuid.clone());
        }
    }

    let id_of = |uuid: &str| state.uuids.iter().find(|(_, u)| *u == uuid).map(|(id, _)| id.clone());
    let mut undeleted = vec![];
    for (host, uuid) in &view.sites {
        // A site of a container deleted elsewhere would point at nothing.
        if deleted.contains(uuid) {
            continue;
        }
        let key = site_key(host);
        let changed = state.baseline.sites.get(host) != Some(uuid);
        let listed = deleted_sites.contains(&key);
        if changed {
            // Assigned here again: no longer deleted, as the add-on does.
            undeleted.push(key.clone());
        } else if seen.contains_key(&key) || listed {
            continue;
        }
        let mut value = seen.get(&key).and_then(Value::as_object).cloned().unwrap_or_default();
        value.insert("userContextId".into(), json!(id_of(uuid).unwrap_or_default()));
        value.entry("neverAsk").or_insert(json!(false));
        value.insert("identityMacAddonUUID".into(), json!(uuid));
        value.insert("hostname".into(), json!(&key[SITE.len()..]));
        set.insert(key, Value::Object(value));
    }
    for host in state.baseline.sites.keys() {
        if !view.sites.contains_key(host) {
            state.deleted_sites.insert(host.clone());
        }
    }
    for host in view.sites.keys() {
        state.deleted_sites.remove(host);
    }
    deleted_sites.retain(|key| !undeleted.contains(key));
    for host in &state.deleted_sites {
        let key = site_key(host);
        if seen.contains_key(&key) {
            remove.push(key.clone());
        }
        if !deleted_sites.contains(&key) {
            deleted_sites.push(key);
        }
    }
    if deleted_ids != ids_before {
        set.insert(DELETED_IDENTITIES.into(), json!(deleted_ids));
    }
    if deleted_sites != sites_before {
        set.insert(DELETED_SITES.into(), json!(deleted_sites));
    }

    // This install, so the add-on keeps the deletions until it has seen them.
    if state.instance.is_empty() {
        state.instance = new_uuid();
    }
    let key = format!("{INSTANCE}{}", state.instance);

    // The store refuses a set that takes the area over storage.sync's quota
    // (webext-storage's api::set), and the collection would never sync again.
    // New site assignments that don't fit stay local, the last hosts first.
    // Our install's key may take up to an item's limit.
    let mut area = seen.clone();
    area.remove(&key);
    for k in &remove {
        area.remove(k);
    }
    area.extend(set.iter().map(|(k, v)| (k.clone(), v.clone())));
    let mut bytes = 1 + SYNC_QUOTA_BYTES_PER_ITEM + area.iter().map(|(k, v)| item_bytes(k, v)).sum::<usize>();
    let new_sites: Vec<String> =
        set.keys().filter(|k| k.starts_with(SITE) && !seen.contains_key(*k)).cloned().collect();
    let mut unsynced = 0;
    for k in new_sites.iter().rev() {
        if area.len() < SYNC_MAX_ITEMS && bytes <= SYNC_QUOTA_BYTES {
            break;
        }
        bytes -= item_bytes(k, &area[k]);
        area.remove(k);
        set.remove(k);
        unsynced += 1;
    }

    let identities: Vec<&String> = view.identities.keys().filter(|u| !deleted.contains(*u)).collect();
    let mut sites: Vec<String> = view
        .sites
        .iter()
        .filter(|(_, uuid)| !deleted.contains(*uuid))
        .map(|(h, _)| site_key(h))
        .filter(|k| area.contains_key(k))
        .collect();
    // Within an item's limit. The add-on reads the list only to keep site
    // deletions until every install has seen them.
    let mut instance_bytes =
        item_bytes(&key, &json!({ "timestamp": now_ms, "identities": identities, "siteAssignments": sites }));
    while instance_bytes > SYNC_QUOTA_BYTES_PER_ITEM {
        let Some(last) = sites.pop() else { break };
        instance_bytes -= Value::from(last).to_string().len() + 1;
    }
    let have = seen.get(&key);
    let stale = have.is_none_or(|v| {
        v["identities"] != json!(identities)
            || v["siteAssignments"] != json!(sites)
            || v["timestamp"].as_i64().unwrap_or(0) + INSTANCE_REFRESH_MS < now_ms
    });
    if stale {
        set.insert(key, json!({ "timestamp": now_ms, "identities": identities, "siteAssignments": sites }));
    }
    (set, remove, unsynced)
}

/// The ops that make the browser's containers and rules match `area`, the
/// store's after the merge, and the count of synced containers. Updates the
/// state to save once the browser applied them.
pub fn finish(local: &Local, state: &mut State, area: &Map<String, Value>) -> (Vec<Op>, usize) {
    let a = Area(area);
    let deleted_ids: HashSet<String> = a.list(DELETED_IDENTITIES).into_iter().collect();
    let deleted_sites: HashSet<String> = a.list(DELETED_SITES).into_iter().collect();
    let ours = format!("{INSTANCE}{}", state.instance);

    // Deletions made here stay pending until the add-on can't bring them
    // back: the lists have them, or no other install does.
    state.deleted_identities.retain(|uuid| {
        let gone = !area.contains_key(&identity_key(uuid));
        let safe = deleted_ids.contains(uuid) || !a.listed_elsewhere(&ours, "identities", uuid);
        !(gone && safe)
    });
    state.deleted_sites.retain(|host| {
        let key = site_key(host);
        let gone = !area.contains_key(&key);
        let safe = deleted_sites.contains(&key) || !a.listed_elsewhere(&ours, "siteAssignments", &key);
        !(gone && safe)
    });

    pair(local, state, area, &deleted_ids);
    let mut target = View::default();
    for (uuid, look) in a.identities() {
        if !deleted_ids.contains(&uuid) && !state.deleted_identities.contains(&uuid) {
            target.identities.insert(uuid, look);
        }
    }
    let listed: BTreeMap<&str, &Container> = local.containers.iter().map(|c| (c.id.as_str(), c)).collect();
    let mut ops = vec![];
    for (uuid, look) in &target.identities {
        let id = match state.uuids.iter().find(|(_, u)| *u == uuid) {
            Some((id, _)) => id.clone(),
            None if valid_id(uuid) && !listed.contains_key(uuid.as_str()) => {
                state.uuids.insert(uuid.clone(), uuid.clone());
                uuid.clone()
            }
            None => continue,
        };
        if listed.get(id.as_str()).map(|c| Look::of(c)) != Some(look.clone()) {
            ops.push(Op::Container {
                id,
                name: look.name.clone(),
                icon: look.icon.clone(),
                color: look.color.clone(),
            });
        }
    }
    target.identities.retain(|uuid, _| state.uuids.values().any(|u| u == uuid));

    for (host, uuid) in a.sites() {
        if target.identities.contains_key(&uuid) && !state.deleted_sites.contains(&host) {
            target.sites.insert(host, uuid);
        }
    }
    let here = local_view(local, &state.uuids);
    for (host, uuid) in &target.sites {
        if here.sites.get(host) != Some(uuid) {
            let id = state.uuids.iter().find(|(_, u)| *u == uuid).map(|(id, _)| id.clone());
            ops.push(Op::Site { host: host.clone(), container: id.unwrap_or_default() });
        }
    }
    for host in here.sites.keys() {
        let key = site_key(host);
        if !target.sites.contains_key(host) && !area.contains_key(&key) && deleted_sites.contains(&key) {
            ops.push(Op::DeleteSite { host: host.clone() });
        }
    }
    // Last: a site's rule is found by its container, which must be listed.
    // A deleted container loses its uuid: if the user keeps it, it goes up
    // again with a new one.
    for c in &local.containers {
        let Some(uuid) = state.uuids.get(&c.id) else { continue };
        if !target.identities.contains_key(uuid) && deleted_ids.contains(uuid) {
            ops.push(Op::DeleteContainer { id: c.id.clone() });
            state.uuids.remove(&c.id);
        }
    }

    let count = target.identities.len();
    state.baseline = target;
    (ops, count)
}

/// What the browser takes as a container id (container.h's IsValidId).
pub fn valid_id(id: &str) -> bool {
    !id.is_empty()
        && id.len() <= 64
        && id.bytes().all(|b| b.is_ascii_alphanumeric() || b == b'-' || b == b'_')
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn hosts_and_rules() {
        for host in ["example.com", "mail.example.com", "xn--bcher-kva.example", "192.168.1.10", "localhost"] {
            assert!(valid_host(host), "{host}");
        }
        // A port glued on by the add-on, IPv4 out of range, upper case, IPv6.
        for host in ["localhost8080", "example.com8080", "192.168.1.300", "Example.com", "", "a..b", "[::1]"] {
            assert!(!valid_host(host), "{host}");
        }
        assert_eq!(rule_host(".example.com"), Some("example.com"));
        assert_eq!(rule_host(" .example.com\n"), Some("example.com"));
        assert_eq!(rule_host(".localhost:8080"), Some("localhost:8080"));
        for filter in ["example.com", ".example.com/path", "https://.example.com", ".a.example\n.b.example",
                       "# .a.example", "*", ".example.com:80", ".example.com:443", ".example.com:08080",
                       ".example.com:70000", ".example.com:", ".192.168.1.10:8080"] {
            assert_eq!(rule_host(filter), None, "{filter}");
        }
        // The add-on's keys: digits after a host are its port.
        assert_eq!(key_site("localhost8080").as_deref(), Some("localhost:8080"));
        assert_eq!(key_site("example.com3000").as_deref(), Some("example.com:3000"));
        assert_eq!(key_site("192.168.1.10").as_deref(), Some("192.168.1.10"));
        for suffix in ["192.168.1.108080", "example.com443", "Example.com8080", "8080"] {
            assert_eq!(key_site(suffix), None, "{suffix}");
        }
    }

    #[test]
    fn reads_the_add_ons_records() {
        // As Multi-Account Containers 8.x writes them (sync.js, assignManager.js).
        let area: Map<String, Value> = serde_json::from_value(json!({
            "identity@@_a0b1c2d3-e4f5-4a6b-8c7d-8e9f0a1b2c3d": {
                "name": "Banking", "icon": "dollar", "color": "gray",
                "cookieStoreId": "firefox-container-3",
                "macAddonUUID": "a0b1c2d3-e4f5-4a6b-8c7d-8e9f0a1b2c3d"},
            "identity@@_broken": {"name": "No uuid of its own", "macAddonUUID": "other"},
            "siteContainerMap@@_www.bank.example": {
                "userContextId": "3", "neverAsk": true,
                "identityMacAddonUUID": "a0b1c2d3-e4f5-4a6b-8c7d-8e9f0a1b2c3d",
                "hostname": "www.bank.example"},
            "siteContainerMap@@_localhost3000": {
                "userContextId": "3", "neverAsk": false,
                "identityMacAddonUUID": "a0b1c2d3-e4f5-4a6b-8c7d-8e9f0a1b2c3d"},
            "deletedIdentityList": ["c1"],
            "deletedSiteList": ["siteContainerMap@@_old.example"],
            "MACinstance:9f8e7d6c-5b4a-4321-8fed-cba987654321": {
                "timestamp": 1730000000000_i64, "identities": ["a0b1c2d3-e4f5-4a6b-8c7d-8e9f0a1b2c3d"],
                "siteAssignments": ["siteContainerMap@@_www.bank.example"]},
        })).unwrap();
        let a = Area(&area);
        let identities = a.identities();
        assert_eq!(identities.len(), 1);
        assert_eq!(identities["a0b1c2d3-e4f5-4a6b-8c7d-8e9f0a1b2c3d"],
                   Look { name: "Banking".into(), icon: "dollar".into(), color: "toolbar".into() });
        let uuid = "a0b1c2d3-e4f5-4a6b-8c7d-8e9f0a1b2c3d".to_string();
        assert_eq!(a.sites().into_iter().collect::<Vec<_>>(),
                   vec![("localhost:3000".to_string(), uuid.clone()), ("www.bank.example".to_string(), uuid)]);
        assert_eq!(a.list(DELETED_IDENTITIES), vec!["c1"]);
        assert!(a.listed_elsewhere("MACinstance:ours", "identities", "a0b1c2d3-e4f5-4a6b-8c7d-8e9f0a1b2c3d"));

        // A first sync pairs Banking here with it by name and takes its site.
        let local: Local = serde_json::from_value(json!({
            "containers": [{"id": "banking", "name": "Banking", "icon": "dollar", "color": "green"}],
            "rules": []})).unwrap();
        let mut state = State::default();
        let (ops, count) = finish(&local, &mut state, &area);
        assert_eq!(count, 1);
        assert_eq!(state.uuids["banking"], "a0b1c2d3-e4f5-4a6b-8c7d-8e9f0a1b2c3d");
        assert_eq!(ops, vec![
            Op::Container { id: "banking".into(), name: "Banking".into(), icon: "dollar".into(),
                            color: "toolbar".into() },
            Op::Site { host: "localhost:3000".into(), container: "banking".into() },
            Op::Site { host: "www.bank.example".into(), container: "banking".into() },
        ]);
    }

    #[test]
    fn sites_on_another_port() {
        // A rule for localhost:8080 here goes up in the add-on's format.
        let uuid = "a0b1c2d3-e4f5-4a6b-8c7d-8e9f0a1b2c3d";
        let local: Local = serde_json::from_value(json!({
            "containers": [{"id": "dev", "name": "Dev", "icon": "fingerprint", "color": "blue"}],
            "rules": [{"url_filter": ".localhost:8080", "container": "dev"}]})).unwrap();
        let mut state = State::default();
        state.uuids.insert("dev".into(), uuid.into());
        state.instance = "ours".into();
        let (set, _, _) = stage(&local, &mut state, &Map::new(), None, 1);
        let site = &set["siteContainerMap@@_localhost8080"];
        assert_eq!(site["hostname"], json!("localhost8080"));
        assert_eq!(site["identityMacAddonUUID"], json!(uuid));
        assert_eq!(set["MACinstance:ours"]["siteAssignments"], json!(["siteContainerMap@@_localhost8080"]));

        // The add-on's localhost:3000 comes down as a rule for that port.
        let mut area = set.clone();
        area.insert("siteContainerMap@@_localhost3000".into(), json!({
            "userContextId": "2", "neverAsk": false, "identityMacAddonUUID": uuid,
            "hostname": "localhost3000"}));
        let (ops, _) = finish(&local, &mut state, &area);
        assert_eq!(ops, vec![Op::Site { host: "localhost:3000".into(), container: "dev".into() }]);
    }

    #[test]
    fn no_sites_of_a_container_deleted_elsewhere() {
        // Firefox deleted Work while a site was assigned to it here.
        let uuid = "a0b1c2d3-e4f5-4a6b-8c7d-8e9f0a1b2c3d";
        let local: Local = serde_json::from_value(json!({
            "containers": [{"id": "work", "name": "Work", "icon": "briefcase", "color": "orange"}],
            "rules": [{"url_filter": ".work.example", "container": "work"}]})).unwrap();
        let mut state = State::default();
        state.uuids.insert("work".into(), uuid.into());
        state.instance = "ours".into();
        let incoming: Map<String, Value> =
            serde_json::from_value(json!({ "deletedIdentityList": [uuid] })).unwrap();
        let (set, _, _) = stage(&local, &mut state, &Map::new(), Some(&incoming), 1);
        assert!(!set.contains_key("siteContainerMap@@_work.example"));
        assert_eq!(set["MACinstance:ours"]["siteAssignments"], json!([]));
    }

    #[test]
    fn a_kept_container_goes_up_again() {
        // Firefox deleted Work; the user keeps it here.
        let uuid = "a0b1c2d3-e4f5-4a6b-8c7d-8e9f0a1b2c3d";
        let local: Local = serde_json::from_value(json!({
            "containers": [{"id": "work", "name": "Work", "icon": "briefcase", "color": "orange"}]}))
        .unwrap();
        let mut state = State::default();
        state.uuids.insert("work".into(), uuid.into());
        state.instance = "ours".into();
        let area: Map<String, Value> =
            serde_json::from_value(json!({ "deletedIdentityList": [uuid] })).unwrap();
        let (ops, _) = finish(&local, &mut state, &area);
        assert_eq!(ops, vec![Op::DeleteContainer { id: "work".into() }]);
        let (set, _, _) = stage(&local, &mut state, &area, None, 1);
        let up: Vec<&String> = set.keys().filter(|k| k.starts_with(IDENTITY)).collect();
        assert_eq!(up.len(), 1);
        assert_ne!(up[0], &identity_key(uuid));
        assert_eq!(set[up[0]]["name"], "Work");
    }

    #[test]
    fn defaults_pair_across_languages() {
        // A Polish profile's first sync with Firefox's English defaults, and a
        // container of its own whose name differs only in case.
        nss::ensure_initialized();
        let uuids = ["a0b1c2d3-e4f5-4a6b-8c7d-8e9f0a1b2c31", "a0b1c2d3-e4f5-4a6b-8c7d-8e9f0a1b2c32",
                     "a0b1c2d3-e4f5-4a6b-8c7d-8e9f0a1b2c33", "a0b1c2d3-e4f5-4a6b-8c7d-8e9f0a1b2c34",
                     "a0b1c2d3-e4f5-4a6b-8c7d-8e9f0a1b2c35"];
        let names = ["Personal", "Work", "Banking", "Shopping", "Family"];
        let mut incoming = Map::new();
        for (uuid, name) in uuids.iter().zip(names) {
            incoming.insert(identity_key(uuid), json!({
                "name": name, "icon": "fingerprint", "color": "blue",
                "cookieStoreId": "firefox-container-1", "macAddonUUID": uuid}));
        }
        let local: Local = serde_json::from_value(json!({"containers": [
            {"id": "personal", "name": "Osobiste", "icon": "fingerprint", "color": "blue"},
            {"id": "work", "name": "Praca", "icon": "briefcase", "color": "orange"},
            {"id": "banking", "name": "Bankowość", "icon": "dollar", "color": "green"},
            {"id": "shopping", "name": "Zakupy", "icon": "cart", "color": "pink"},
            {"id": "family", "name": "family", "icon": "tree", "color": "red"},
        ]})).unwrap();
        let mut state = State::default();
        state.instance = "ours".into();
        let (set, _, _) = stage(&local, &mut state, &Map::new(), Some(&incoming), 1);
        for (id, uuid) in ["personal", "work", "banking", "shopping", "family"].iter().zip(uuids) {
            assert_eq!(state.uuids[*id], uuid, "{id}");
        }
        let new: Vec<&String> =
            set.keys().filter(|k| k.starts_with(IDENTITY) && !incoming.contains_key(*k)).collect();
        assert!(new.is_empty(), "{new:?}");
    }

    #[test]
    fn new_uuids_are_uuidv4() {
        nss::ensure_initialized();
        let uuid = new_uuid();
        assert!(is_uuid(&uuid), "{uuid}");
        assert_eq!(&uuid[14..15], "4");
        assert_ne!(uuid, new_uuid());
    }
}
