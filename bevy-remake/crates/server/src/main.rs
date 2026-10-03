use bevy::{app::ScheduleRunnerPlugin, prelude::*};
use bfv_shared::*;
use std::{
    collections::HashMap,
    net::{SocketAddr, UdpSocket},
    time::{Duration, Instant},
};

struct Peer {
    id: u32,
    token: u64,
    nonce: u64,
    seen: Instant,
}
#[derive(Resource)]
struct Server {
    socket: UdpSocket,
    peers: HashMap<SocketAddr, Peer>,
    sim: Simulation,
    bot_target: usize,
    started: Instant,
    exit_after: Option<Duration>,
}
fn main() {
    let args: Vec<String> = std::env::args().collect();
    let value = |flag: &str| args.windows(2).find(|a| a[0] == flag).map(|a| a[1].clone());
    if args.iter().any(|a| a == "--help") {
        println!(
            "bfv-server [--bind 0.0.0.0:23000] [--bots 8] [--exit-after SECONDS]\n16 total actors; authoritative 60 Hz simulation, 20 Hz snapshots."
        );
        return;
    }
    let bind = value("--bind").unwrap_or(format!("0.0.0.0:{PORT}"));
    let bots: usize = value("--bots")
        .unwrap_or("8".into())
        .parse()
        .expect("--bots must be an integer");
    let exit_after = value("--exit-after")
        .map(|s| Duration::from_secs(s.parse().expect("invalid --exit-after")));
    let socket = UdpSocket::bind(&bind).unwrap_or_else(|e| panic!("Cannot bind {bind}: {e}"));
    socket.set_nonblocking(true).expect("nonblocking socket");
    println!(
        "BFV Bevy conquest server | {bind} | protocol {PROTOCOL} | bots {}",
        bots.min(MAX_ACTORS)
    );
    App::new()
        .add_plugins(
            MinimalPlugins.set(ScheduleRunnerPlugin::run_loop(Duration::from_secs_f64(
                1. / 120.,
            ))),
        )
        .insert_resource(Time::<Fixed>::from_hz(TICK_RATE as f64))
        .insert_resource(Server {
            socket,
            peers: HashMap::new(),
            sim: Simulation::default(),
            bot_target: bots.min(MAX_ACTORS),
            started: Instant::now(),
            exit_after,
        })
        .add_systems(FixedUpdate, tick)
        .run();
}
fn send(socket: &UdpSocket, address: SocketAddr, packet: &ServerPacket) {
    if let Ok(bytes) = encode(packet) {
        let _ = socket.send_to(&bytes, address);
    }
}
fn tick(mut server: ResMut<Server>, mut exit: MessageWriter<AppExit>) {
    let s = &mut *server;
    if s.exit_after
        .is_some_and(|duration| s.started.elapsed() >= duration)
    {
        exit.write(AppExit::Success);
        return;
    }
    let mut buffer = [0u8; MAX_PACKET + 1];
    // Bound receive work so flooding cannot monopolize a simulation tick.
    for _ in 0..256 {
        let (size, address) = match s.socket.recv_from(&mut buffer) {
            Ok(result) => result,
            Err(e) if e.kind() == std::io::ErrorKind::WouldBlock => break,
            Err(_) => break,
        };
        if size > MAX_PACKET {
            continue;
        }
        let Ok(packet) = decode::<ClientPacket>(&buffer[..size]) else {
            continue;
        };
        match packet {
            ClientPacket::Hello { protocol, nonce } => {
                if protocol != PROTOCOL {
                    send(
                        &s.socket,
                        address,
                        &ServerPacket::Rejected { nonce, reason: 1 },
                    );
                    continue;
                }
                if let Some(peer) = s.peers.get(&address) {
                    if peer.nonce != nonce {
                        continue;
                    }
                    send(
                        &s.socket,
                        address,
                        &ServerPacket::Welcome {
                            protocol: PROTOCOL,
                            nonce,
                            id: peer.id,
                            token: peer.token,
                        },
                    );
                    continue;
                }
                if s.sim.world.actors.len() == MAX_ACTORS
                    && let Some(id) = s.sim.world.actors.iter().find(|a| a.bot).map(|a| a.id)
                {
                    s.sim.remove_actor(id);
                }
                let Some(id) = s.sim.add_actor(false) else {
                    send(
                        &s.socket,
                        address,
                        &ServerPacket::Rejected { nonce, reason: 2 },
                    );
                    continue;
                };
                let mut bytes = [0u8; 8];
                getrandom::fill(&mut bytes).expect("OS random session token");
                let token = u64::from_le_bytes(bytes);
                s.peers.insert(
                    address,
                    Peer {
                        id,
                        token,
                        nonce,
                        seen: Instant::now(),
                    },
                );
                send(
                    &s.socket,
                    address,
                    &ServerPacket::Welcome {
                        protocol: PROTOCOL,
                        nonce,
                        id,
                        token,
                    },
                );
                println!("Joined: player {id} from {address}");
            }
            ClientPacket::Input { token, input } => {
                if let Some(peer) = s.peers.get_mut(&address)
                    && peer.token == token
                    && s.sim.submit(peer.id, input)
                {
                    peer.seen = Instant::now();
                }
            }
            ClientPacket::Goodbye { token } => {
                if s.peers.get(&address).is_some_and(|p| p.token == token) {
                    let peer = s.peers.remove(&address).unwrap();
                    s.sim.remove_actor(peer.id);
                    println!("Left: player {}", peer.id);
                }
            }
        }
    }
    let expired: Vec<_> = s
        .peers
        .iter()
        .filter(|(_, p)| p.seen.elapsed() > Duration::from_secs(5))
        .map(|(a, _)| *a)
        .collect();
    for address in expired {
        if let Some(peer) = s.peers.remove(&address) {
            s.sim.remove_actor(peer.id);
            println!("Timed out: player {}", peer.id);
        }
    }
    let target = (s.bot_target + s.peers.len()).min(MAX_ACTORS);
    while s.sim.world.actors.len() < target {
        s.sim.add_actor(true);
    }
    s.sim.step();
    if s.sim.world.tick.is_multiple_of(3) {
        for (address, peer) in &s.peers {
            send(
                &s.socket,
                *address,
                &ServerPacket::Snapshot {
                    token: peer.token,
                    world: s.sim.world.clone(),
                },
            );
        }
    }
}
