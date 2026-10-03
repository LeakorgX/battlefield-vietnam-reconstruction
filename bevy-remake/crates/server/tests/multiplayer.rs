use bfv_shared::*;
use std::{
    net::UdpSocket,
    process::{Child, Command, Stdio},
    thread,
    time::{Duration, Instant},
};

struct ServerProcess(Child);
impl Drop for ServerProcess {
    fn drop(&mut self) {
        let _ = self.0.kill();
        let _ = self.0.wait();
    }
}
struct Client {
    socket: UdpSocket,
    id: u32,
    token: u64,
}
impl Client {
    fn send(&self, packet: ClientPacket) {
        self.socket.send(&encode(&packet).unwrap()).unwrap();
    }
    fn receive(&self) -> Option<ServerPacket> {
        let mut bytes = [0u8; MAX_PACKET + 1];
        self.socket.recv(&mut bytes).ok().and_then(|n| {
            assert!(n <= MAX_PACKET);
            decode(&bytes[..n]).ok()
        })
    }
    fn connect(address: &str, nonce: u64) -> Self {
        let socket = UdpSocket::bind("127.0.0.1:0").unwrap();
        socket.connect(address).unwrap();
        socket
            .set_read_timeout(Some(Duration::from_millis(100)))
            .unwrap();
        let mut client = Self {
            socket,
            id: 0,
            token: 0,
        };
        let deadline = Instant::now() + Duration::from_secs(5);
        while Instant::now() < deadline {
            client.send(ClientPacket::Hello {
                protocol: PROTOCOL,
                nonce,
            });
            if let Some(ServerPacket::Welcome { id, token, .. }) = client.receive() {
                client.id = id;
                client.token = token;
                return client;
            }
        }
        panic!("Server did not accept client");
    }
    fn snapshot(&self, tick: u64) -> Snapshot {
        let deadline = Instant::now() + Duration::from_secs(3);
        while Instant::now() < deadline {
            if let Some(ServerPacket::Snapshot { token, world }) = self.receive() {
                assert_eq!(token, self.token);
                if world.tick > tick {
                    return world;
                }
            }
        }
        panic!("No fresh snapshot");
    }
}
#[test]
fn two_real_udp_clients_share_authoritative_world_and_bad_tokens_cannot_control_it() {
    let reservation = UdpSocket::bind("127.0.0.1:0").unwrap();
    let address = reservation.local_addr().unwrap().to_string();
    drop(reservation);
    let mut command = Command::new(env!("CARGO_BIN_EXE_bfv-server"));
    command
        .args(["--bind", &address, "--bots", "0", "--exit-after", "15"])
        .stdout(Stdio::null())
        .stderr(Stdio::null());
    #[cfg(windows)]
    {
        use std::os::windows::process::CommandExt;
        command.creation_flags(0x08000000);
    }
    let _server = ServerProcess(command.spawn().unwrap());
    let first = Client::connect(&address, 100);
    let second = Client::connect(&address, 200);
    assert_ne!(first.id, second.id);
    assert_ne!(first.token, second.token);
    let initial = first.snapshot(0);
    let start = initial
        .actors
        .iter()
        .find(|a| a.id == first.id)
        .unwrap()
        .position;
    for sequence in 1..=20 {
        first.send(ClientPacket::Input {
            token: first.token,
            input: Input {
                sequence,
                forward: 1.,
                yaw: -std::f32::consts::FRAC_PI_2,
                fire: true,
                ..Default::default()
            },
        });
        second.send(ClientPacket::Input {
            token: second.token,
            input: Input {
                sequence,
                ..Default::default()
            },
        });
        thread::sleep(Duration::from_millis(17));
    }
    let mut moved = second.snapshot(initial.tick);
    // Drain snapshots queued during movement and observe a tick after all inputs.
    while moved.tick < initial.tick + 20 {
        moved = second.snapshot(moved.tick);
    }
    assert_eq!(moved.actors.len(), 2);
    let actor = moved.actors.iter().find(|a| a.id == first.id).unwrap();
    assert!(actor.position[0] > start[0] + 1.);
    assert!(actor.ammo < 30);
    assert!(actor.ack > 0);
    first.send(ClientPacket::Input {
        token: first.token ^ 1,
        input: Input {
            sequence: 9000,
            forward: 1.,
            ..Default::default()
        },
    });
    let mut fresh = first.snapshot(moved.tick);
    while fresh.tick < moved.tick + 4 {
        fresh = first.snapshot(fresh.tick);
    }
    assert!(fresh.actors.iter().find(|a| a.id == first.id).unwrap().ack < 9000);
    first.send(ClientPacket::Goodbye { token: first.token });
    let mut left = second.snapshot(fresh.tick);
    while left.actors.len() != 1 {
        left = second.snapshot(left.tick);
    }
    assert_eq!(left.actors[0].id, second.id);
    let new_client = Client::connect(&address, 300);
    assert_ne!(new_client.id, first.id);
    let bad = UdpSocket::bind("127.0.0.1:0").unwrap();
    bad.connect(&address).unwrap();
    bad.set_read_timeout(Some(Duration::from_secs(1))).unwrap();
    bad.send(
        &encode(&ClientPacket::Hello {
            protocol: 999,
            nonce: 999,
        })
        .unwrap(),
    )
    .unwrap();
    let mut buffer = [0; MAX_PACKET];
    let n = bad.recv(&mut buffer).unwrap();
    assert!(matches!(
        decode::<ServerPacket>(&buffer[..n]).unwrap(),
        ServerPacket::Rejected {
            nonce: 999,
            reason: 1
        }
    ));
}
