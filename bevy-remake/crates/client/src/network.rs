use bevy::prelude::*;
use bfv_shared::*;
use std::{
    collections::VecDeque,
    net::{ToSocketAddrs, UdpSocket},
    time::{Duration, Instant, SystemTime, UNIX_EPOCH},
};

#[derive(Resource)]
pub struct Connection {
    socket: UdpSocket,
    pub address: String,
    nonce: u64,
    pub id: u32,
    token: u64,
    pub world: Option<Snapshot>,
    pub previous: Option<Snapshot>,
    pub input: Input,
    pub predicted: [f32; 3],
    pending: VecDeque<Input>,
    pub received: Instant,
    hello: Instant,
    pub status: String,
    pub scoreboard: bool,
    rejected: bool,
}
impl Connection {
    pub fn new(address: &str) -> std::io::Result<Self> {
        let remote = address
            .to_socket_addrs()?
            .next()
            .ok_or(std::io::Error::other("No server address"))?;
        let socket = UdpSocket::bind(if remote.is_ipv4() {
            "0.0.0.0:0"
        } else {
            "[::]:0"
        })?;
        socket.connect(remote)?;
        socket.set_nonblocking(true)?;
        Ok(Self {
            socket,
            address: address.into(),
            nonce: SystemTime::now()
                .duration_since(UNIX_EPOCH)
                .unwrap()
                .as_nanos() as u64,
            id: 0,
            token: 0,
            world: None,
            previous: None,
            input: Input::default(),
            predicted: [-78., 0., 0.],
            pending: VecDeque::new(),
            received: Instant::now(),
            hello: Instant::now() - Duration::from_secs(1),
            status: "Connecting...".into(),
            scoreboard: false,
            rejected: false,
        })
    }
    fn send(&self, packet: &ClientPacket) {
        if let Ok(bytes) = encode(packet) {
            let _ = self.socket.send(&bytes);
        }
    }
}
impl Drop for Connection {
    fn drop(&mut self) {
        if self.id != 0 {
            self.send(&ClientPacket::Goodbye { token: self.token });
        }
    }
}
fn predict(position: &mut [f32; 3], input: Input) {
    let length = (input.forward.powi(2) + input.strafe.powi(2))
        .sqrt()
        .max(1.);
    let speed = if input.sprint { 9. } else { 5.5 };
    move_horizontal(
        position,
        (-input.yaw.sin() * input.forward + input.yaw.cos() * input.strafe) / length * speed * DT,
        (-input.yaw.cos() * input.forward - input.yaw.sin() * input.strafe) / length * speed * DT,
        0.4,
    );
}
pub fn send_input(mut connection: ResMut<Connection>) {
    let c = &mut *connection;
    if c.id == 0 {
        if !c.rejected && c.hello.elapsed() > Duration::from_millis(500) {
            c.send(&ClientPacket::Hello {
                protocol: PROTOCOL,
                nonce: c.nonce,
            });
            c.hello = Instant::now();
        }
        return;
    }
    c.input.sequence = c.input.sequence.wrapping_add(1);
    c.send(&ClientPacket::Input {
        token: c.token,
        input: c.input,
    });
    if c.world
        .as_ref()
        .filter(|w| w.winner == 0)
        .and_then(|w| w.actors.iter().find(|a| a.id == c.id))
        .is_some_and(|a| a.health > 0 && a.vehicle < 0)
    {
        c.pending.push_back(c.input);
        if c.pending.len() > 120 {
            c.pending.pop_front();
        }
        predict(&mut c.predicted, c.input);
    }
}
pub fn receive(mut connection: ResMut<Connection>) {
    let c = &mut *connection;
    let mut buffer = [0u8; MAX_PACKET + 1];
    for _ in 0..64 {
        let size = match c.socket.recv(&mut buffer) {
            Ok(n) => n,
            Err(_) => break,
        };
        if size > MAX_PACKET {
            continue;
        }
        let Ok(packet) = decode::<ServerPacket>(&buffer[..size]) else {
            continue;
        };
        match packet {
            ServerPacket::Welcome {
                protocol,
                nonce,
                id,
                token,
            } if protocol == PROTOCOL && nonce == c.nonce && c.id == 0 => {
                c.id = id;
                c.token = token;
                c.input.sequence = 0;
                c.received = Instant::now();
                c.status = "Connected".into();
            }
            ServerPacket::Snapshot { token, world } if token == c.token && c.id != 0 => {
                if c.world.as_ref().is_some_and(|w| world.tick <= w.tick) {
                    continue;
                }
                if let Some(a) = world.actors.iter().find(|a| a.id == c.id) {
                    if c.world.is_none() {
                        c.input.yaw = a.yaw;
                        c.input.pitch = a.pitch;
                    }
                    c.predicted = a.position;
                    while c.pending.front().is_some_and(|i| i.sequence <= a.ack) {
                        c.pending.pop_front();
                    }
                    if a.health > 0 && a.vehicle < 0 && world.winner == 0 {
                        for input in &c.pending {
                            predict(&mut c.predicted, *input);
                        }
                    } else {
                        c.pending.clear();
                    }
                }
                c.previous = c.world.take();
                c.world = Some(world);
                c.received = Instant::now();
                c.status = "Connected".into();
            }
            ServerPacket::Rejected { nonce, reason } if nonce == c.nonce => {
                c.rejected = true;
                c.status = if reason == 1 {
                    "Server protocol mismatch".into()
                } else {
                    "Server is full (16 players)".into()
                };
            }
            _ => {}
        }
    }
    if c.id != 0 && c.received.elapsed() > Duration::from_secs(6) {
        c.id = 0;
        c.token = 0;
        c.world = None;
        c.previous = None;
        c.pending.clear();
        c.status = "Connection lost - reconnecting...".into();
    }
}
