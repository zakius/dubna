// Copyright 2026 The oganesson Authors
// SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

//! The `extension-storage` collection: Firefox add-ons' `storage.sync` areas,
//! one record per add-on (by Gecko ID), over application-services'
//! webext-storage store and engine, which Firefox runs too. This engine is
//! the collection's one owner: each area the browser has a mapping for is
//! staged from the browser's snapshot before the store merges the server's
//! records, and read back into ops after. So far that is Multi-Account
//! Containers' area (mac.rs). Other add-ons' records stay in the store as
//! they came.
//!
//! The browser's `local` is `{<Gecko ID>: snapshot}`, and each op carries
//! the Gecko ID of its area in `ext`.
//!
//! Firefox syncs this collection only while Add-ons is on in its Sync
//! settings: until meta/global lists the collection, nothing is staged and
//! the sync reports why.

use std::cell::{Cell, RefCell};
use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};

use anyhow::{anyhow, bail, Context, Result};
use serde::{Deserialize, Serialize};
use serde_json::{json, Map, Value};
use sync15::bso::{IncomingBso, OutgoingBso, OutgoingEnvelope};
use sync15::engine::{CollSyncIds, CollectionRequest, EngineSyncAssociation, SyncEngine};
use sync15::{telemetry, CollectionName, Guid, ServerTimestamp};
use webext_storage::{WebExtStorageBridgedEngine, WebExtStorageStore};

use crate::mac;

const COLLECTION: &str = "extension-storage";

/// Persisted by the browser once it applied the ops.
#[derive(Serialize, Deserialize, Default)]
struct State {
    /// meta/global's sync ID, which the store's engine leaves to its caller.
    #[serde(default)]
    global: String,
    #[serde(default)]
    mac: mac::State,
}

fn store_path(profile: &str) -> PathBuf {
    Path::new(profile).join("firefox_sync_extension_storage.db")
}

/// Deletes the profile's store (on sign-out).
pub fn remove_store(profile: &str) {
    crate::remove_db(&store_path(profile));
}

fn area(store: &WebExtStorageStore, ext: &str) -> Result<Map<String, Value>> {
    match store.get(ext, Value::Null)? {
        Value::Object(map) => Ok(map),
        _ => Ok(Map::new()),
    }
}

fn now_ms() -> i64 {
    SystemTime::now().duration_since(UNIX_EPOCH).map_or(0, |d| d.as_millis() as i64)
}

pub struct ExtensionStorageEngine {
    store: Arc<WebExtStorageStore>,
    bridge: Arc<WebExtStorageBridgedEngine>,
    global: RefCell<String>,
    mac_local: mac::Local,
    mac_state: RefCell<mac::State>,
    /// The areas this sync downloaded, by Gecko ID.
    incoming: RefCell<HashMap<String, Map<String, Value>>>,
    /// Whether the collection synced: meta/global lists it.
    started: Cell<bool>,
}

impl ExtensionStorageEngine {
    pub fn new(profile: &str, args: &Value) -> Result<Self> {
        let state: State = match args["state"].as_str() {
            Some(s) if !s.is_empty() => serde_json::from_str(s).context("bad engine state")?,
            _ => State::default(),
        };
        let local = args["local"].as_object().ok_or_else(|| anyhow!("bad extension storage"))?;
        if let Some(ext) = local.keys().find(|ext| *ext != mac::EXT_ID) {
            bail!("no storage.sync mapping for {ext}");
        }
        let mac_local: mac::Local = match local.get(mac::EXT_ID) {
            Some(snapshot) => serde_json::from_value(snapshot.clone()).context("bad containers")?,
            None => bail!("no containers"),
        };
        let store = Arc::new(WebExtStorageStore::new(store_path(profile))?);
        let bridge = store.clone().bridged_engine();
        Ok(Self {
            store,
            bridge,
            global: RefCell::new(state.global),
            mac_local,
            mac_state: RefCell::new(state.mac),
            incoming: RefCell::new(HashMap::new()),
            started: Cell::new(false),
        })
    }
}

impl crate::Collection for ExtensionStorageEngine {
    fn finish(self: Box<Self>) -> Result<Value> {
        if !self.started.get() {
            bail!(
                "Firefox doesn't sync add-on data on this account: turn on Add-ons in \
                 Firefox's Sync settings"
            );
        }
        let mut state = self.mac_state.take();
        let area = area(&self.store, mac::EXT_ID)?;
        let (ops, count) = mac::finish(&self.mac_local, &mut state, &area);
        let mut tagged = vec![];
        for op in ops {
            let mut op = serde_json::to_value(op)?;
            op["ext"] = json!(mac::EXT_ID);
            tagged.push(op);
        }
        let state = State { global: self.global.take(), mac: state };
        Ok(json!({ "state": serde_json::to_string(&state)?, "ops": tagged, "count": count }))
    }
}

impl SyncEngine for ExtensionStorageEngine {
    fn collection_name(&self) -> CollectionName {
        COLLECTION.into()
    }

    fn sync_started(&self) -> Result<()> {
        self.started.set(true);
        self.bridge.sync_started()
    }

    fn stage_incoming(&self, inbound: Vec<IncomingBso>, _telem: &mut telemetry::Engine) -> Result<()> {
        let mut records = Vec::with_capacity(inbound.len());
        for bso in inbound {
            let payload: Value = serde_json::from_str(&bso.payload).unwrap_or_default();
            if let (Some(ext), Some(data)) = (payload["extId"].as_str(), payload["data"].as_str()) {
                if let Ok(Value::Object(area)) = serde_json::from_str(data) {
                    self.incoming.borrow_mut().insert(ext.to_string(), area);
                }
            }
            // The bridge takes the BSOs as Firefox's JS hands them over: JSON.
            records.push(
                json!({
                    "id": bso.envelope.id,
                    "modified": bso.envelope.modified,
                    "sortindex": bso.envelope.sortindex,
                    "ttl": bso.envelope.ttl,
                    "payload": bso.payload,
                })
                .to_string(),
            );
        }
        self.bridge.store_incoming(records)
    }

    fn apply(&self, timestamp: ServerTimestamp, _telem: &mut telemetry::Engine) -> Result<Vec<OutgoingBso>> {
        let current = area(&self.store, mac::EXT_ID)?;
        let incoming = self.incoming.borrow();
        let (set, remove) = mac::stage(
            &self.mac_local,
            &mut self.mac_state.borrow_mut(),
            &current,
            incoming.get(mac::EXT_ID),
            now_ms(),
        );
        if !remove.is_empty() {
            self.store.remove(mac::EXT_ID, json!(remove))?;
        }
        if !set.is_empty() {
            self.store.set(mac::EXT_ID, Value::Object(set))?;
        }
        let mut outgoing = vec![];
        for record in self.bridge.apply(timestamp.as_millis())? {
            let record: Value = serde_json::from_str(&record)?;
            let id = record["id"].as_str().ok_or_else(|| anyhow!("outgoing record without id"))?;
            outgoing.push(OutgoingBso {
                envelope: OutgoingEnvelope {
                    id: Guid::new(id),
                    sortindex: record["sortindex"].as_i64().map(|n| n as i32),
                    ttl: record["ttl"].as_u64().map(|n| n as u32),
                },
                payload: record["payload"].as_str().unwrap_or_default().to_string(),
            });
        }
        Ok(outgoing)
    }

    fn set_uploaded(&self, new_timestamp: ServerTimestamp, ids: Vec<Guid>) -> Result<()> {
        let ids = ids.into_iter().map(|id| id.to_string()).collect();
        self.bridge.set_uploaded(new_timestamp.as_millis(), ids)
    }

    fn sync_finished(&self) -> Result<()> {
        self.bridge.sync_finished()
    }

    fn get_collection_request(&self, server: ServerTimestamp) -> Result<Option<CollectionRequest>> {
        let since = self.bridge.last_sync()?;
        if since == server.as_millis() {
            return Ok(None);
        }
        Ok(Some(CollectionRequest::new(COLLECTION.into()).full().newer_than(ServerTimestamp(since))))
    }

    fn get_sync_assoc(&self) -> Result<EngineSyncAssociation> {
        let global = self.global.borrow();
        Ok(match self.bridge.sync_id()? {
            Some(coll) if !global.is_empty() => EngineSyncAssociation::Connected(CollSyncIds {
                global: Guid::new(&global),
                coll: Guid::new(&coll),
            }),
            _ => EngineSyncAssociation::Disconnected,
        })
    }

    fn reset(&self, assoc: &EngineSyncAssociation) -> Result<()> {
        self.bridge.reset()?;
        match assoc {
            EngineSyncAssociation::Disconnected => self.global.borrow_mut().clear(),
            EngineSyncAssociation::Connected(ids) => {
                self.bridge.ensure_current_sync_id(ids.coll.as_str())?;
                *self.global.borrow_mut() = ids.global.to_string();
            }
        }
        Ok(())
    }

    fn wipe(&self) -> Result<()> {
        self.bridge.wipe()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::Collection;
    use sync15::bso::IncomingEnvelope;

    const OTHER_EXT: &str = "uBlock0@raymondhill.net";

    /// The server's extension-storage collection: payloads by record id,
    /// with their modified time (ms).
    #[derive(Default)]
    struct Server {
        records: HashMap<String, (Value, i64)>,
        now: i64,
    }

    impl Server {
        fn area_of(&self, ext: &str) -> Option<(String, Map<String, Value>)> {
            self.records.iter().find(|(_, (p, _))| p["extId"] == ext).map(|(id, (p, _))| {
                (id.clone(), serde_json::from_str(p["data"].as_str().unwrap()).unwrap())
            })
        }

        fn mac(&self) -> Map<String, Value> {
            self.area_of(mac::EXT_ID).map(|(_, area)| area).unwrap_or_default()
        }

        /// Another device (Firefox with the add-on) changes an area and
        /// uploads it.
        fn firefox(&mut self, ext: &str, edit: impl FnOnce(&mut Map<String, Value>)) {
            let new_id = if ext == mac::EXT_ID { "firefoxmac01" } else { "firefoxubo01" };
            let (id, mut area) = self.area_of(ext).unwrap_or((new_id.into(), Map::new()));
            edit(&mut area);
            self.now += 1000;
            let payload = json!({"id": id, "extId": ext, "data": Value::Object(area).to_string()});
            self.records.insert(id, (payload, self.now));
        }
    }

    /// An oganesson device: its profile, its containers and rules, and the
    /// engine state it saved.
    struct Device {
        profile: String,
        local: Value,
        state: String,
    }

    impl Device {
        fn new(name: &str, local: Value) -> Self {
            nss::ensure_initialized();
            let dir = std::env::temp_dir().join(format!("og-ext-{name}-{}", std::process::id()));
            std::fs::create_dir_all(&dir).unwrap();
            let profile = dir.to_str().unwrap().to_string();
            remove_store(&profile);
            Device { profile, local, state: String::new() }
        }

        /// One sync. With `upload_fails`, the merge happens and the upload
        /// fails, so there's no reply. Out: `{state, ops, count}` and the
        /// number of records uploaded.
        fn run(&self, server: &mut Server, upload_fails: bool) -> Option<(Value, usize)> {
            let args = json!({"state": self.state, "local": {mac::EXT_ID: self.local}});
            let engine = ExtensionStorageEngine::new(&self.profile, &args).unwrap();
            let ids = CollSyncIds { global: Guid::new("globalsyncid"), coll: Guid::new("collsyncid01") };
            match engine.get_sync_assoc().unwrap() {
                EngineSyncAssociation::Connected(have) if have == ids => {}
                _ => engine.reset(&EngineSyncAssociation::Connected(ids)).unwrap(),
            }
            let mut telem = telemetry::Engine::new(COLLECTION);
            engine.sync_started().unwrap();
            let since = engine.bridge.last_sync().unwrap();
            if engine.get_collection_request(ServerTimestamp(server.now)).unwrap().is_some() {
                let incoming = server
                    .records
                    .iter()
                    .filter(|(_, (_, modified))| *modified > since)
                    .map(|(id, (payload, modified))| {
                        let envelope = IncomingEnvelope {
                            id: Guid::new(id),
                            modified: ServerTimestamp(*modified),
                            sortindex: None,
                            ttl: None,
                        };
                        IncomingBso::new(envelope, payload.to_string())
                    })
                    .collect();
                engine.stage_incoming(incoming, &mut telem).unwrap();
            }
            let outgoing = engine.apply(ServerTimestamp(server.now), &mut telem).unwrap();
            if upload_fails {
                return None;
            }
            let uploaded = outgoing.len();
            if uploaded > 0 {
                server.now += 1000;
                let mut ids = vec![];
                for bso in outgoing {
                    let payload: Value = serde_json::from_str(&bso.payload).unwrap();
                    server.records.insert(bso.envelope.id.to_string(), (payload, server.now));
                    ids.push(bso.envelope.id);
                }
                engine.set_uploaded(ServerTimestamp(server.now), ids).unwrap();
            }
            engine.sync_finished().unwrap();
            Some((Box::new(engine).finish().unwrap(), uploaded))
        }

        /// A sync whose ops the browser applies and whose state it saves.
        fn sync(&mut self, server: &mut Server) -> Vec<Value> {
            let (out, _) = self.run(server, false).unwrap();
            let ops = out["ops"].as_array().unwrap().clone();
            for op in &ops {
                apply(&mut self.local, op);
            }
            self.state = out["state"].as_str().unwrap().into();
            ops
        }

        /// Syncs until nothing goes up: our install's key follows what
        /// came down one sync later. Then nothing must change.
        fn settle(&mut self, server: &mut Server) {
            for _ in 0..2 {
                let (out, uploaded) = self.run(server, false).unwrap();
                self.state = out["state"].as_str().unwrap().into();
                assert_eq!(out["ops"], json!([]));
                if uploaded == 0 {
                    return;
                }
            }
            panic!("still uploading");
        }

        fn container(&self, name: &str) -> Option<Value> {
            self.local["containers"].as_array().unwrap().iter().find(|c| c["name"] == name).cloned()
        }

        fn edit(&mut self, id: &str, field: &str, value: &str) {
            for c in self.local["containers"].as_array_mut().unwrap() {
                if c["id"] == id {
                    c[field] = json!(value);
                }
            }
        }
    }

    impl Drop for Device {
        fn drop(&mut self) {
            remove_store(&self.profile);
        }
    }

    /// What the browser's adapter does with an op (containers_adapter.cc).
    fn apply(local: &mut Value, op: &Value) {
        assert_eq!(op["ext"], mac::EXT_ID);
        let ids: Vec<Value> = local["containers"].as_array().unwrap().iter().map(|c| c["id"].clone()).collect();
        let host = format!(".{}", op["host"].as_str().unwrap_or_default());
        let synced = |r: &Value| r["url_filter"] == host.as_str() && r["enabled"] == true && ids.contains(&r["container"]);
        match op["op"].as_str().unwrap() {
            "container" => {
                let fields = json!({"id": op["id"], "name": op["name"], "icon": op["icon"], "color": op["color"]});
                let list = local["containers"].as_array_mut().unwrap();
                match list.iter_mut().find(|c| c["id"] == op["id"]) {
                    Some(c) => *c = fields,
                    None => list.push(fields),
                }
            }
            "delete_container" => local["containers"].as_array_mut().unwrap().retain(|c| c["id"] != op["id"]),
            "site" => {
                let rules = local["rules"].as_array_mut().unwrap();
                match rules.iter_mut().find(|r| synced(r)) {
                    Some(r) => r["container"] = op["container"].clone(),
                    None => rules.push(json!({"url_filter": host, "container": op["container"], "enabled": true})),
                }
            }
            "delete_site" => local["rules"].as_array_mut().unwrap().retain(|r| !synced(r)),
            other => panic!("unknown op {other}"),
        }
    }

    fn defaults() -> Value {
        json!({
            "containers": [
                {"id": "personal", "name": "Personal", "icon": "fingerprint", "color": "blue"},
                {"id": "work", "name": "Work", "icon": "briefcase", "color": "orange"},
            ],
            "rules": [
                {"url_filter": ".news.example", "container": "personal", "enabled": true},
                // Rules the add-on can't express stay local.
                {"url_filter": "example.org", "container": "work", "enabled": true},
                {"url_filter": ".tmp.example", "container": "temporary", "enabled": true},
                {"url_filter": ".off.example", "container": "work", "enabled": false},
                {"url_filter": ".none.example", "container": "", "enabled": true},
            ],
        })
    }

    const FX_WORK: &str = "0e6a1c9b-5f3d-4a8e-9c2b-7d1e3f4a5b6c";

    /// Firefox's area as the add-on writes it: its Work container, a site
    /// assignment, its install, and a site on a port, which stays there.
    fn firefox_area(area: &mut Map<String, Value>) {
        area.insert(format!("identity@@_{FX_WORK}"), json!({
            "name": "Work", "icon": "briefcase", "color": "red",
            "cookieStoreId": "firefox-container-2", "macAddonUUID": FX_WORK}));
        area.insert("siteContainerMap@@_mail.example.com".into(), json!({
            "userContextId": "2", "neverAsk": false, "identityMacAddonUUID": FX_WORK,
            "hostname": "mail.example.com"}));
        area.insert("siteContainerMap@@_localhost8080".into(), json!({
            "userContextId": "2", "neverAsk": false, "identityMacAddonUUID": FX_WORK,
            "hostname": "localhost8080"}));
        area.insert("MACinstance:5d2f7c1e-firefox".into(), json!({
            "timestamp": 1, "identities": [FX_WORK],
            "siteAssignments": ["siteContainerMap@@_mail.example.com", "siteContainerMap@@_localhost8080"]}));
    }

    fn uuid_of(area: &Map<String, Value>, name: &str) -> String {
        area.iter()
            .find(|(k, v)| k.starts_with("identity@@_") && v["name"] == name)
            .map(|(_, v)| v["macAddonUUID"].as_str().unwrap().to_string())
            .unwrap_or_else(|| panic!("no {name} in {area:?}"))
    }

    fn setup(name: &str) -> (Server, Device) {
        let mut server = Server::default();
        server.firefox(mac::EXT_ID, firefox_area);
        server.firefox(OTHER_EXT, |a| {
            a.insert("selectedFilterLists".into(), json!(["easylist"]));
        });
        let mut device = Device::new(name, defaults());
        device.sync(&mut server);
        (server, device)
    }

    #[test]
    fn first_sync_pairs_by_name_and_writes_the_add_ons_format() {
        let mut server = Server::default();
        server.firefox(mac::EXT_ID, firefox_area);
        server.firefox(OTHER_EXT, |a| {
            a.insert("selectedFilterLists".into(), json!(["easylist"]));
        });
        let other_before = server.records["firefoxubo01"].clone();
        let mut device = Device::new("first", defaults());
        let ops = device.sync(&mut server);

        // Work is Firefox's Work: Firefox's color wins, and its site comes in.
        assert_eq!(ops, vec![
            json!({"ext": mac::EXT_ID, "op": "container", "id": "work", "name": "Work",
                   "icon": "briefcase", "color": "red"}),
            json!({"ext": mac::EXT_ID, "op": "site", "host": "mail.example.com", "container": "work"}),
        ]);
        let area = server.mac();
        assert_eq!(uuid_of(&area, "Work"), FX_WORK);
        assert_eq!(area.keys().filter(|k| k.starts_with("identity@@_")).count(), 2);
        let personal = uuid_of(&area, "Personal");
        assert_eq!(area[&format!("identity@@_{personal}")], json!({
            "name": "Personal", "icon": "fingerprint", "color": "blue",
            "macAddonUUID": personal, "cookieStoreId": "firefox-container-personal"}));
        assert_eq!(area["siteContainerMap@@_news.example"], json!({
            "userContextId": "personal", "neverAsk": false, "identityMacAddonUUID": personal,
            "hostname": "news.example"}));
        // Local-only rules didn't go up; Firefox's site on a port stayed.
        let sites: Vec<&String> = area.keys().filter(|k| k.starts_with("siteContainerMap@@_")).collect();
        assert_eq!(sites.len(), 3);
        assert!(area.contains_key("siteContainerMap@@_localhost8080"));
        let instance = area.iter().find(|(k, _)| k.starts_with("MACinstance:") && !k.contains("firefox")).unwrap().1;
        assert_eq!(instance["identities"].as_array().unwrap().len(), 2);
        // Another add-on's record is the same as Firefox's.
        assert_eq!(server.area_of(OTHER_EXT).unwrap().1["selectedFilterLists"], json!(["easylist"]));
        assert_eq!(server.records["firefoxubo01"], other_before);
        // Then nothing changes on either side.
        device.settle(&mut server);
        let (out, uploaded) = device.run(&mut server, false).unwrap();
        assert_eq!((out["ops"].clone(), out["count"].clone(), uploaded), (json!([]), json!(2), 0));
    }

    #[test]
    fn edits_on_each_side_and_on_both() {
        let (mut server, mut device) = setup("edits");
        let personal = uuid_of(&server.mac(), "Personal");
        // Firefox renames Work and recolors it; here Personal gets a new icon.
        server.firefox(mac::EXT_ID, |a| {
            a.insert(format!("identity@@_{FX_WORK}"), json!({"name": "Job", "icon": "dollar",
                "color": "cyan", "cookieStoreId": "firefox-container-2", "macAddonUUID": FX_WORK}));
        });
        device.edit("personal", "icon", "tree");
        let ops = device.sync(&mut server);
        // Firefox 153's cyan arrives as the add-on's turquoise.
        assert_eq!(ops, vec![json!({"ext": mac::EXT_ID, "op": "container", "id": "work", "name": "Job",
                                    "icon": "dollar", "color": "turquoise"})]);
        let area = server.mac();
        assert_eq!(area[&format!("identity@@_{personal}")]["icon"], "tree");
        assert_eq!(area[&format!("identity@@_{FX_WORK}")]["cookieStoreId"], "firefox-container-2");

        // Both rename Work: Firefox's name wins, as in the add-on.
        server.firefox(mac::EXT_ID, |a| {
            a[&format!("identity@@_{FX_WORK}")]["name"] = json!("Desk");
        });
        device.edit("work", "name", "Office");
        device.sync(&mut server);
        assert_eq!(device.container("Desk").unwrap()["id"], "work");
        assert_eq!(server.mac()[&format!("identity@@_{FX_WORK}")]["name"], "Desk");

        // Firefox moves a site to Personal; here a site moves to Work.
        server.firefox(mac::EXT_ID, |a| {
            a["siteContainerMap@@_mail.example.com"]["identityMacAddonUUID"] = json!(personal);
        });
        for rule in device.local["rules"].as_array_mut().unwrap() {
            if rule["url_filter"] == ".news.example" {
                rule["container"] = json!("work");
            }
        }
        let ops = device.sync(&mut server);
        assert_eq!(ops, vec![json!({"ext": mac::EXT_ID, "op": "site", "host": "mail.example.com",
                                    "container": "personal"})]);
        assert_eq!(server.mac()["siteContainerMap@@_news.example"]["identityMacAddonUUID"], FX_WORK);
    }

    #[test]
    fn deletions_on_each_side() {
        let (mut server, mut device) = setup("deletions");
        let personal = uuid_of(&server.mac(), "Personal");
        // Deleted here: its key goes and the add-on's list has it. Its rule
        // now opens no container of the list, so its site goes too.
        device.local["containers"].as_array_mut().unwrap().retain(|c| c["id"] != "personal");
        device.sync(&mut server);
        let area = server.mac();
        assert!(!area.contains_key(&format!("identity@@_{personal}")));
        assert_eq!(area["deletedIdentityList"], json!([personal]));
        assert!(!area.contains_key("siteContainerMap@@_news.example"));
        assert_eq!(area["deletedSiteList"], json!(["siteContainerMap@@_news.example"]));

        // Deleted in Firefox, as the add-on does: key, list, its sites.
        server.firefox(mac::EXT_ID, |a| {
            a.remove(&format!("identity@@_{FX_WORK}"));
            a["deletedIdentityList"].as_array_mut().unwrap().push(json!(FX_WORK));
            a.remove("siteContainerMap@@_mail.example.com");
            a["deletedSiteList"].as_array_mut().unwrap().push(json!("siteContainerMap@@_mail.example.com"));
        });
        let ops = device.sync(&mut server);
        assert_eq!(ops, vec![
            json!({"ext": mac::EXT_ID, "op": "delete_site", "host": "mail.example.com"}),
            json!({"ext": mac::EXT_ID, "op": "delete_container", "id": "work"}),
        ]);
        // The rules the add-on can't express are still there.
        assert_eq!(device.local["rules"].as_array().unwrap().len(), 5);
        device.settle(&mut server);
        let (out, uploaded) = device.run(&mut server, false).unwrap();
        assert_eq!((out["ops"].clone(), out["count"].clone(), uploaded), (json!([]), json!(0), 0));
    }

    #[test]
    fn a_retry_keeps_remote_changes_the_browser_never_applied() {
        // oganesson-core#40: the browser quits before it applies a sync's
        // ops, or the upload fails after the merge. The next sync, from the
        // last saved state with the same snapshot, must not take the old
        // values for local edits.
        let (mut server, device) = setup("retry");
        let personal = uuid_of(&server.mac(), "Personal");
        server.firefox(mac::EXT_ID, |a| {
            let work = &mut a[&format!("identity@@_{FX_WORK}")];
            work["name"] = json!("Job");
            work["icon"] = json!("dollar");
            work["color"] = json!("green");
            a.remove(&format!("identity@@_{personal}"));
            a.insert("deletedIdentityList".into(), json!([personal]));
            a["siteContainerMap@@_mail.example.com"]["identityMacAddonUUID"] = json!(personal);
        });
        let expected = vec![
            json!({"ext": mac::EXT_ID, "op": "container", "id": "work", "name": "Job", "icon": "dollar",
                   "color": "green"}),
            json!({"ext": mac::EXT_ID, "op": "delete_container", "id": "personal"}),
        ];
        // Never applied, state not saved.
        let (first, _) = device.run(&mut server, false).unwrap();
        assert_eq!(first["ops"], json!(expected));
        // A sync whose upload fails after the merge.
        assert!(device.run(&mut server, true).is_none());
        let (retry, _) = device.run(&mut server, false).unwrap();
        assert_eq!(retry["ops"], json!(expected));
        let area = server.mac();
        assert_eq!(area[&format!("identity@@_{FX_WORK}")]["name"], "Job");
        assert_eq!(area[&format!("identity@@_{FX_WORK}")]["color"], "green");
        assert!(!area.contains_key(&format!("identity@@_{personal}")));
    }

    #[test]
    fn two_devices_see_the_same_containers_and_sites() {
        let mut server = Server::default();
        server.firefox(mac::EXT_ID, firefox_area);
        let mut a = Device::new("two-a", defaults());
        let mut b = Device::new("two-b", json!({"containers": [], "rules": []}));
        a.sync(&mut server);
        let travel = "6f1d2c3b-4a5e-4f60-8a7b-9c0d1e2f3a4b";
        a.local["containers"].as_array_mut().unwrap().push(json!({"id": travel, "name": "Travel",
            "icon": "vacation", "color": "pink"}));
        a.local["rules"].as_array_mut().unwrap().push(json!({"url_filter": ".trip.example",
            "container": travel, "enabled": true}));
        a.sync(&mut server);
        assert_eq!(uuid_of(&server.mac(), "Travel"), travel);

        b.sync(&mut server);
        let names: Vec<&str> = b.local["containers"].as_array().unwrap().iter()
            .map(|c| c["name"].as_str().unwrap()).collect();
        assert_eq!(names.len(), 3);
        for name in ["Personal", "Work", "Travel"] {
            assert!(names.contains(&name), "{name} in {names:?}");
        }
        // Containers from elsewhere take the add-on's uuid as their id.
        assert_eq!(b.container("Work").unwrap()["id"], FX_WORK);
        assert_eq!(b.container("Travel").unwrap()["id"], travel);
        let rules: Vec<(String, String)> = b.local["rules"].as_array().unwrap().iter()
            .map(|r| (r["url_filter"].as_str().unwrap().into(), r["container"].as_str().unwrap().into()))
            .collect();
        assert!(rules.contains(&(".trip.example".into(), travel.into())));
        assert!(rules.contains(&(".mail.example.com".into(), FX_WORK.into())));
        assert_eq!(rules.len(), 3);
        // Neither has more to do: no record goes up, no op comes back.
        for _ in 0..2 {
            a.settle(&mut server);
            b.settle(&mut server);
        }
        for device in [&a, &b] {
            let (out, uploaded) = device.run(&mut server, false).unwrap();
            assert_eq!((out["ops"].clone(), uploaded), (json!([]), 0));
        }

        // B renames Travel; A gets it.
        b.edit(travel, "name", "Trips");
        b.sync(&mut server);
        a.sync(&mut server);
        assert_eq!(a.container("Trips").unwrap()["id"], travel);
    }

    #[test]
    fn a_deletion_the_list_lost_stays_deleted() {
        // Firefox changes deletedIdentityList in the same sync as a deletion
        // here, so the merge keeps Firefox's list, without ours; Firefox,
        // still having the container, uploads it again.
        let (mut server, mut device) = setup("lost");
        let personal = uuid_of(&server.mac(), "Personal");
        // Firefox has Personal too, and deletes another container.
        server.firefox(mac::EXT_ID, |a| {
            a["MACinstance:5d2f7c1e-firefox"]["identities"] = json!([FX_WORK, personal]);
        });
        device.sync(&mut server);
        server.firefox(mac::EXT_ID, |a| {
            a.insert("deletedIdentityList".into(), json!(["11111111-2222-4333-8444-555555555555"]));
        });
        device.local["containers"].as_array_mut().unwrap().retain(|c| c["id"] != "personal");
        device.sync(&mut server);
        let area = server.mac();
        assert!(!area["deletedIdentityList"].as_array().unwrap().contains(&json!(personal)));
        let personal_value = json!({"name": "Personal", "icon": "fingerprint", "color": "blue",
            "cookieStoreId": "firefox-container-1", "macAddonUUID": personal});
        server.firefox(mac::EXT_ID, |a| {
            a.insert(format!("identity@@_{personal}"), personal_value.clone());
        });
        // It doesn't come back here. This sync lists it as deleted again;
        // Firefox's copy wins the merge, so the next one removes it.
        let ops = device.sync(&mut server);
        assert!(ops.iter().all(|op| op["name"] != "Personal"), "{ops:?}");
        assert!(server.mac()["deletedIdentityList"].as_array().unwrap().contains(&json!(personal)));
        let ops = device.sync(&mut server);
        assert!(ops.iter().all(|op| op["name"] != "Personal"), "{ops:?}");
        let area = server.mac();
        assert!(!area.contains_key(&format!("identity@@_{personal}")));
        assert!(area["deletedIdentityList"].as_array().unwrap().contains(&json!(personal)));
        assert!(device.container("Personal").is_none());
    }

    #[test]
    fn not_synced_until_firefox_syncs_add_ons() {
        let device = Device::new("declined", defaults());
        let args = json!({"state": "", "local": {mac::EXT_ID: device.local}});
        let engine = ExtensionStorageEngine::new(&device.profile, &args).unwrap();
        let error = Box::new(engine).finish().unwrap_err().to_string();
        assert!(error.contains("Add-ons"), "{error}");
        // Only areas there is a mapping for.
        let args = json!({"state": "", "local": {OTHER_EXT: {}}});
        assert!(ExtensionStorageEngine::new(&device.profile, &args).is_err());
    }
}
