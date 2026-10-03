//! Rendering-independent protocol, map and authoritative conquest simulation.
use bincode::Options;
use serde::{Deserialize, Serialize};
use std::collections::BTreeMap;

pub const PROTOCOL: u16 = 1;
pub const PORT: u16 = 23000;
pub const TICK_RATE: f32 = 60.0;
pub const DT: f32 = 1.0 / TICK_RATE;
pub const MAX_ACTORS: usize = 16;
pub const MAX_PACKET: usize = 1200;
pub const MAP_LIMIT: f32 = 95.0;
pub const FLAG_POSITIONS: [[f32; 3]; 3] = [[-42., 0., 12.], [0., 0., -16.], [42., 0., 12.]];
pub const FLAG_NAMES: [&str; 3] = ["RIVER OUTPOST", "VILLAGE", "HILL CAMP"];

#[derive(Clone, Copy, Debug)]
pub struct Block {
    pub center: [f32; 3],
    pub half: [f32; 3],
}
pub const BLOCKS: [Block; 12] = [
    Block {
        center: [-15., 2., -12.],
        half: [5., 2., 4.],
    },
    Block {
        center: [15., 2., -12.],
        half: [5., 2., 4.],
    },
    Block {
        center: [-15., 2., 8.],
        half: [5., 2., 4.],
    },
    Block {
        center: [15., 2., 8.],
        half: [5., 2., 4.],
    },
    Block {
        center: [-54., 1.5, -8.],
        half: [4., 1.5, 5.],
    },
    Block {
        center: [54., 1.5, -8.],
        half: [4., 1.5, 5.],
    },
    Block {
        center: [-35., 0.7, 25.],
        half: [8., 0.7, 0.8],
    },
    Block {
        center: [35., 0.7, 25.],
        half: [8., 0.7, 0.8],
    },
    Block {
        center: [-24., 0.7, -38.],
        half: [0.8, 0.7, 8.],
    },
    Block {
        center: [24., 0.7, -38.],
        half: [0.8, 0.7, 8.],
    },
    Block {
        center: [-5., 0.9, 36.],
        half: [3., 0.9, 1.],
    },
    Block {
        center: [5., 0.9, 36.],
        half: [3., 0.9, 1.],
    },
];

#[derive(Clone, Copy, Default, Debug, Serialize, Deserialize)]
pub struct Input {
    pub sequence: u32,
    pub forward: f32,
    pub strafe: f32,
    pub yaw: f32,
    pub pitch: f32,
    pub sprint: bool,
    pub jump: bool,
    pub fire: bool,
    pub reload: bool,
    pub interact: bool,
}
impl Input {
    pub fn valid(&self) -> bool {
        [self.forward, self.strafe, self.yaw, self.pitch]
            .iter()
            .all(|v| v.is_finite())
            && self.forward.abs() <= 1.0
            && self.strafe.abs() <= 1.0
            && self.yaw.abs() <= 1000.0
            && self.pitch.abs() <= 1.5
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub enum ClientPacket {
    Hello { protocol: u16, nonce: u64 },
    Input { token: u64, input: Input },
    Goodbye { token: u64 },
}
#[derive(Clone, Debug, Serialize, Deserialize)]
pub enum ServerPacket {
    Welcome {
        protocol: u16,
        nonce: u64,
        id: u32,
        token: u64,
    },
    Snapshot {
        token: u64,
        world: Snapshot,
    },
    Rejected {
        nonce: u64,
        reason: u8,
    },
}

pub fn encode<T: Serialize>(packet: &T) -> Result<Vec<u8>, bincode::Error> {
    bincode::DefaultOptions::new()
        .with_fixint_encoding()
        .with_limit(MAX_PACKET as u64)
        .serialize(packet)
}
pub fn decode<T: serde::de::DeserializeOwned>(bytes: &[u8]) -> Result<T, bincode::Error> {
    bincode::DefaultOptions::new()
        .with_fixint_encoding()
        .with_limit(MAX_PACKET as u64)
        .reject_trailing_bytes()
        .deserialize(bytes)
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Actor {
    pub id: u32,
    pub team: u8,
    pub bot: bool,
    pub position: [f32; 3],
    pub yaw: f32,
    pub pitch: f32,
    pub health: u8,
    pub ammo: u8,
    pub reloading: bool,
    pub kills: u16,
    pub deaths: u16,
    pub ack: u32,
    pub vehicle: i8,
    pub shot: u16,
}
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct Flag {
    pub owner: u8,
    pub progress: f32,
    pub contested: bool,
}
#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct Jeep {
    pub position: [f32; 3],
    pub yaw: f32,
    pub driver: u32,
}
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct Snapshot {
    pub tick: u64,
    pub actors: Vec<Actor>,
    pub flags: [Flag; 3],
    pub jeeps: [Jeep; 2],
    pub tickets: [u16; 2],
    pub winner: u8,
    pub round: u32,
}
#[derive(Default)]
struct Runtime {
    input: Input,
    input_tick: u64,
    vy: f32,
    cooldown: f32,
    reload: f32,
    respawn: f32,
    interact_down: bool,
}
pub struct Simulation {
    pub world: Snapshot,
    runtime: BTreeMap<u32, Runtime>,
    next_id: u32,
    round_end: f32,
}
impl Default for Simulation {
    fn default() -> Self {
        Self {
            world: Snapshot {
                tick: 0,
                actors: Vec::new(),
                flags: [
                    Flag {
                        owner: 1,
                        progress: 1.,
                        contested: false,
                    },
                    Flag {
                        owner: 0,
                        progress: 0.,
                        contested: false,
                    },
                    Flag {
                        owner: 2,
                        progress: -1.,
                        contested: false,
                    },
                ],
                jeeps: initial_jeeps(),
                tickets: [200, 200],
                winner: 0,
                round: 1,
            },
            runtime: BTreeMap::new(),
            next_id: 1,
            round_end: 0.,
        }
    }
}
fn initial_jeeps() -> [Jeep; 2] {
    [
        Jeep {
            position: [-68., 0., 12.],
            yaw: -std::f32::consts::FRAC_PI_2,
            driver: 0,
        },
        Jeep {
            position: [68., 0., 12.],
            yaw: std::f32::consts::FRAC_PI_2,
            driver: 0,
        },
    ]
}
pub fn spawn_position(team: u8, id: u32) -> [f32; 3] {
    [
        if team == 1 { -78. } else { 78. },
        0.,
        (id % 5) as f32 * 3. - 6.,
    ]
}
pub fn horizontal_distance(a: [f32; 3], b: [f32; 3]) -> f32 {
    ((a[0] - b[0]).powi(2) + (a[2] - b[2]).powi(2)).sqrt()
}
pub fn direction(yaw: f32, pitch: f32) -> [f32; 3] {
    [
        -yaw.sin() * pitch.cos(),
        pitch.sin(),
        -yaw.cos() * pitch.cos(),
    ]
}
pub fn blocked(position: [f32; 3], radius: f32) -> bool {
    position[0].abs() > MAP_LIMIT - radius
        || position[2].abs() > MAP_LIMIT - radius
        || BLOCKS.iter().any(|b| {
            (position[0] - b.center[0]).abs() < b.half[0] + radius
                && (position[2] - b.center[2]).abs() < b.half[2] + radius
                && position[1] < b.center[1] + b.half[1]
        })
}
pub fn move_horizontal(position: &mut [f32; 3], dx: f32, dz: f32, radius: f32) {
    let mut p = *position;
    p[0] += dx;
    if !blocked(p, radius) {
        position[0] = p[0];
    }
    p = *position;
    p[2] += dz;
    if !blocked(p, radius) {
        position[2] = p[2];
    }
}
/// Slab intersection, shared by weapon occlusion and tests.
pub fn ray_box(origin: [f32; 3], dir: [f32; 3], center: [f32; 3], half: [f32; 3]) -> Option<f32> {
    let mut near: f32 = 0.;
    let mut far: f32 = 160.;
    for axis in 0..3 {
        let low = center[axis] - half[axis];
        let high = center[axis] + half[axis];
        if dir[axis].abs() < 0.00001 {
            if origin[axis] < low || origin[axis] > high {
                return None;
            }
        } else {
            let a = (low - origin[axis]) / dir[axis];
            let b = (high - origin[axis]) / dir[axis];
            near = near.max(a.min(b));
            far = far.min(a.max(b));
            if near > far {
                return None;
            }
        }
    }
    Some(near)
}

impl Simulation {
    pub fn add_actor(&mut self, bot: bool) -> Option<u32> {
        if self.world.actors.len() >= MAX_ACTORS {
            return None;
        }
        let counts = [
            self.world.actors.iter().filter(|a| a.team == 1).count(),
            self.world.actors.iter().filter(|a| a.team == 2).count(),
        ];
        let team = if counts[0] <= counts[1] { 1 } else { 2 };
        let id = self.next_id;
        self.next_id += 1;
        self.world.actors.push(Actor {
            id,
            team,
            bot,
            position: spawn_position(team, id),
            yaw: if team == 1 {
                -std::f32::consts::FRAC_PI_2
            } else {
                std::f32::consts::FRAC_PI_2
            },
            pitch: 0.,
            health: 100,
            ammo: 30,
            reloading: false,
            kills: 0,
            deaths: 0,
            ack: 0,
            vehicle: -1,
            shot: 0,
        });
        self.runtime.insert(id, Runtime::default());
        Some(id)
    }
    pub fn remove_actor(&mut self, id: u32) {
        self.world.actors.retain(|a| a.id != id);
        self.runtime.remove(&id);
        for jeep in &mut self.world.jeeps {
            if jeep.driver == id {
                jeep.driver = 0;
            }
        }
    }
    pub fn submit(&mut self, id: u32, input: Input) -> bool {
        if !input.valid() {
            return false;
        }
        let Some(runtime) = self.runtime.get_mut(&id) else {
            return false;
        };
        if input.sequence <= runtime.input.sequence {
            return false;
        }
        runtime.input = input;
        runtime.input_tick = self.world.tick;
        true
    }
    fn bot_inputs(&mut self) {
        for a in &self.world.actors {
            if !a.bot || a.health == 0 {
                continue;
            }
            let target = FLAG_POSITIONS
                .iter()
                .enumerate()
                .filter(|(i, _)| self.world.flags[*i].owner != a.team)
                .min_by(|(_, p), (_, q)| {
                    horizontal_distance(a.position, **p)
                        .total_cmp(&horizontal_distance(a.position, **q))
                })
                .map(|(_, p)| *p)
                .unwrap_or(FLAG_POSITIONS[1]);
            let enemy = self
                .world
                .actors
                .iter()
                .filter(|e| e.team != a.team && e.health > 0)
                .filter(|e| horizontal_distance(e.position, a.position) < 45.)
                .filter(|e| {
                    let mut eye = a.position;
                    eye[1] += 1.65;
                    let dx = e.position[0] - a.position[0];
                    let dz = e.position[2] - a.position[2];
                    let yaw = (-dx).atan2(-dz);
                    !BLOCKS.iter().any(|b| {
                        ray_box(eye, direction(yaw, 0.), b.center, b.half)
                            .is_some_and(|d| d < horizontal_distance(e.position, a.position))
                    })
                })
                .min_by(|e, f| {
                    horizontal_distance(a.position, e.position)
                        .total_cmp(&horizontal_distance(a.position, f.position))
                });
            let aim = enemy.map(|e| e.position).unwrap_or(target);
            let yaw = (a.position[0] - aim[0]).atan2(a.position[2] - aim[2]);
            let r = self.runtime.get_mut(&a.id).unwrap();
            r.input = Input {
                sequence: r.input.sequence + 1,
                forward: if enemy.is_some() {
                    0.2
                } else if horizontal_distance(a.position, target) > 4. {
                    1.
                } else {
                    0.
                },
                strafe: if blocked(
                    [
                        a.position[0] - yaw.sin() * 1.5,
                        a.position[1],
                        a.position[2] - yaw.cos() * 1.5,
                    ],
                    0.4,
                ) {
                    1.
                } else {
                    0.
                },
                yaw,
                pitch: 0.,
                fire: enemy.is_some(),
                reload: a.ammo == 0,
                ..Default::default()
            };
            r.input_tick = self.world.tick;
        }
    }
    pub fn step(&mut self) {
        self.world.tick += 1;
        if self.world.winner != 0 {
            self.round_end += DT;
            if self.round_end > 12. {
                self.restart();
            }
            return;
        }
        self.bot_inputs();
        let mut shots = Vec::new();
        for a in &mut self.world.actors {
            let r = self.runtime.get_mut(&a.id).unwrap();
            if a.health == 0 {
                r.respawn -= DT;
                if r.respawn <= 0. {
                    a.position = spawn_position(a.team, a.id);
                    a.health = 100;
                    a.ammo = 30;
                    a.reloading = false;
                    r.vy = 0.;
                    r.reload = 0.;
                    r.cooldown = 0.;
                }
                continue;
            }
            // A lost connection must not leave movement or firing held forever.
            let input = if self.world.tick - r.input_tick <= 15 {
                r.input
            } else {
                Input {
                    yaw: a.yaw,
                    pitch: a.pitch,
                    ..Default::default()
                }
            };
            a.ack = r.input.sequence;
            a.yaw = input.yaw;
            a.pitch = input.pitch;
            r.cooldown = (r.cooldown - DT).max(0.);
            if r.reload > 0. {
                r.reload -= DT;
                if r.reload <= 0. {
                    a.ammo = 30;
                    a.reloading = false;
                }
            }
            if input.reload && a.ammo < 30 && r.reload <= 0. {
                r.reload = 2.;
                a.reloading = true;
            }
            if input.interact && !r.interact_down {
                if a.vehicle >= 0 {
                    let jeep = &mut self.world.jeeps[a.vehicle as usize];
                    let candidates = [
                        [jeep.position[0] + 3., 0., jeep.position[2]],
                        [jeep.position[0] - 3., 0., jeep.position[2]],
                        [jeep.position[0], 0., jeep.position[2] + 3.],
                    ];
                    if let Some(p) = candidates.into_iter().find(|p| !blocked(*p, 0.4)) {
                        a.position = p;
                        jeep.driver = 0;
                        a.vehicle = -1;
                    }
                } else if let Some((index, jeep)) =
                    self.world.jeeps.iter_mut().enumerate().find(|(_, j)| {
                        j.driver == 0 && horizontal_distance(j.position, a.position) < 4.
                    })
                {
                    jeep.driver = a.id;
                    a.vehicle = index as i8;
                }
            }
            r.interact_down = input.interact;
            if a.vehicle >= 0 {
                let j = &mut self.world.jeeps[a.vehicle as usize];
                j.yaw -= input.strafe * 1.8 * DT;
                move_horizontal(
                    &mut j.position,
                    -j.yaw.sin() * input.forward * 19. * DT,
                    -j.yaw.cos() * input.forward * 19. * DT,
                    1.5,
                );
                a.position = j.position;
                a.position[1] = 0.65;
            } else {
                let length = (input.forward.powi(2) + input.strafe.powi(2))
                    .sqrt()
                    .max(1.);
                let speed = if input.sprint { 9. } else { 5.5 };
                let dx = (-a.yaw.sin() * input.forward + a.yaw.cos() * input.strafe) / length;
                let dz = (-a.yaw.cos() * input.forward - a.yaw.sin() * input.strafe) / length;
                if input.jump && a.position[1] <= 0. {
                    r.vy = 6.;
                }
                r.vy -= 18. * DT;
                a.position[1] = (a.position[1] + r.vy * DT).max(0.);
                if a.position[1] == 0. {
                    r.vy = 0.;
                }
                move_horizontal(&mut a.position, dx * speed * DT, dz * speed * DT, 0.4);
            }
            if input.fire && a.ammo > 0 && r.cooldown <= 0. && r.reload <= 0. && a.vehicle < 0 {
                a.ammo -= 1;
                a.shot = a.shot.wrapping_add(1);
                r.cooldown = if a.bot { 0.55 } else { 0.12 };
                let mut eye = a.position;
                eye[1] += 1.65;
                shots.push((a.id, a.team, eye, direction(a.yaw, a.pitch)));
            }
        }
        for (shooter, team, eye, dir) in shots {
            let wall = BLOCKS
                .iter()
                .filter_map(|b| ray_box(eye, dir, b.center, b.half))
                .fold(160., f32::min);
            let hit = self
                .world
                .actors
                .iter()
                .filter(|a| a.team != team && a.health > 0)
                .filter_map(|a| {
                    let mut center = a.position;
                    center[1] += 0.9;
                    ray_box(eye, dir, center, [0.4, 0.9, 0.4])
                        .filter(|d| *d < wall)
                        .map(|d| (a.id, d))
                })
                .min_by(|(_, a), (_, b)| a.total_cmp(b))
                .map(|(id, _)| id);
            if let Some(id) = hit {
                let a = self.world.actors.iter_mut().find(|a| a.id == id).unwrap();
                a.health = a.health.saturating_sub(34);
                if a.health == 0 {
                    a.deaths += 1;
                    self.runtime.get_mut(&id).unwrap().respawn = 4.;
                    self.world.tickets[(a.team - 1) as usize] =
                        self.world.tickets[(a.team - 1) as usize].saturating_sub(1);
                    if a.vehicle >= 0 {
                        self.world.jeeps[a.vehicle as usize].driver = 0;
                        a.vehicle = -1;
                    }
                    if let Some(killer) = self.world.actors.iter_mut().find(|a| a.id == shooter) {
                        killer.kills += 1;
                    }
                }
            }
        }
        for (i, flag) in self.world.flags.iter_mut().enumerate() {
            let mut presence = [0u8; 2];
            for a in &self.world.actors {
                if a.health > 0 && horizontal_distance(a.position, FLAG_POSITIONS[i]) < 9. {
                    presence[(a.team - 1) as usize] += 1;
                }
            }
            flag.contested = presence[0] > 0 && presence[1] > 0;
            if !flag.contested {
                let change = if presence[0] > 0 {
                    1.
                } else if presence[1] > 0 {
                    -1.
                } else {
                    0.
                };
                flag.progress = (flag.progress + change * DT / 8.).clamp(-1., 1.);
                if flag.owner == 1 && flag.progress <= 0. || flag.owner == 2 && flag.progress >= 0.
                {
                    flag.owner = 0;
                }
                if flag.progress >= 1. {
                    flag.owner = 1;
                }
                if flag.progress <= -1. {
                    flag.owner = 2;
                }
            }
        }
        if self.world.tick.is_multiple_of(60) {
            let held = [
                self.world.flags.iter().filter(|f| f.owner == 1).count(),
                self.world.flags.iter().filter(|f| f.owner == 2).count(),
            ];
            if held[0] >= 2 {
                self.world.tickets[1] = self.world.tickets[1].saturating_sub((held[0] - 1) as u16);
            }
            if held[1] >= 2 {
                self.world.tickets[0] = self.world.tickets[0].saturating_sub((held[1] - 1) as u16);
            }
        }
        if self.world.tickets[0] == 0 {
            self.world.winner = 2;
        } else if self.world.tickets[1] == 0 {
            self.world.winner = 1;
        }
    }
    fn restart(&mut self) {
        self.round_end = 0.;
        self.world.round += 1;
        self.world.winner = 0;
        self.world.tickets = [200, 200];
        self.world.flags = Self::default().world.flags;
        self.world.jeeps = initial_jeeps();
        for a in &mut self.world.actors {
            a.position = spawn_position(a.team, a.id);
            a.health = 100;
            a.ammo = 30;
            a.reloading = false;
            a.vehicle = -1;
            a.kills = 0;
            a.deaths = 0;
            let r = self.runtime.get_mut(&a.id).unwrap();
            let sequence = r.input.sequence;
            *r = Runtime::default();
            r.input.sequence = sequence;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn maximum_snapshot_fits_without_ip_fragmentation() {
        let mut sim = Simulation::default();
        for _ in 0..MAX_ACTORS {
            sim.add_actor(false).unwrap();
        }
        let wire = encode(&ServerPacket::Snapshot {
            token: 42,
            world: sim.world,
        })
        .unwrap();
        assert!(wire.len() <= MAX_PACKET, "{} bytes", wire.len());
        assert!(decode::<ServerPacket>(&wire).is_ok());
    }
    #[test]
    fn rejects_invalid_and_replayed_input_and_normalizes_diagonals() {
        let mut sim = Simulation::default();
        let id = sim.add_actor(false).unwrap();
        let start = sim.world.actors[0].position;
        assert!(!sim.submit(
            id,
            Input {
                sequence: 1,
                yaw: f32::NAN,
                ..Default::default()
            }
        ));
        assert!(sim.submit(
            id,
            Input {
                sequence: 1,
                forward: 1.,
                strafe: 1.,
                ..Default::default()
            }
        ));
        assert!(!sim.submit(
            id,
            Input {
                sequence: 1,
                ..Default::default()
            }
        ));
        sim.step();
        assert!(
            (horizontal_distance(start, sim.world.actors[0].position) - 5.5 * DT).abs() < 0.0001
        );
    }
    #[test]
    fn packet_flood_cannot_multiply_movement_and_stale_input_stops() {
        let mut sim = Simulation::default();
        let id = sim.add_actor(false).unwrap();
        let start = sim.world.actors[0].position;
        for sequence in 1..1000 {
            sim.submit(
                id,
                Input {
                    sequence,
                    forward: 1.,
                    ..Default::default()
                },
            );
        }
        sim.step();
        assert!(horizontal_distance(start, sim.world.actors[0].position) < 0.1);
        for _ in 0..20 {
            sim.step();
        }
        let stopped = sim.world.actors[0].position;
        for _ in 0..60 {
            sim.step();
        }
        assert_eq!(stopped, sim.world.actors[0].position);
    }
    #[test]
    fn rifle_kill_respawn_and_reload_are_server_owned() {
        let mut sim = Simulation::default();
        let id = sim.add_actor(false).unwrap();
        sim.add_actor(false);
        sim.world.actors[0].position = [-70., 0., 0.];
        sim.world.actors[1].position = [-70., 0., -10.];
        for sequence in 1..=30 {
            sim.submit(
                id,
                Input {
                    sequence,
                    fire: true,
                    ..Default::default()
                },
            );
            sim.step();
        }
        assert_eq!(sim.world.actors[1].health, 0);
        assert_eq!(sim.world.actors[0].kills, 1);
        assert_eq!(sim.world.tickets[1], 199);
        sim.submit(
            id,
            Input {
                sequence: 31,
                reload: true,
                ..Default::default()
            },
        );
        for _ in 0..250 {
            sim.step();
        }
        assert_eq!(sim.world.actors[0].ammo, 30);
        assert_eq!(sim.world.actors[1].health, 100);
    }
    #[test]
    fn cover_occludes_rifle_and_collision_prevents_wall_crossing() {
        let mut sim = Simulation::default();
        let id = sim.add_actor(false).unwrap();
        sim.add_actor(false);
        sim.world.actors[0].position = [-15., 0., 0.];
        sim.world.actors[1].position = [-15., 0., -22.];
        sim.submit(
            id,
            Input {
                sequence: 1,
                fire: true,
                ..Default::default()
            },
        );
        sim.step();
        assert_eq!(sim.world.actors[1].health, 100);
        let mut p = [-15., 0., -7.];
        move_horizontal(&mut p, 0., -2., 0.4);
        assert_eq!(p[2], -7.);
    }
    #[test]
    fn contested_flags_do_not_capture_and_majority_bleeds_tickets() {
        let mut sim = Simulation::default();
        let a = sim.add_actor(false).unwrap();
        let b = sim.add_actor(false).unwrap();
        sim.world.actors[0].position = FLAG_POSITIONS[1];
        sim.world.actors[1].position = FLAG_POSITIONS[1];
        for _ in 0..600 {
            sim.step();
        }
        assert!(sim.world.flags[1].contested);
        assert_eq!(sim.world.flags[1].progress, 0.);
        sim.remove_actor(b);
        for _ in 0..540 {
            sim.step();
        }
        assert_eq!(sim.world.flags[1].owner, 1);
        assert!(sim.world.tickets[1] < 200);
        assert!(sim.runtime.contains_key(&a));
    }
    #[test]
    fn vehicle_exclusivity_exit_and_disconnect_release() {
        let mut sim = Simulation::default();
        let id = sim.add_actor(false).unwrap();
        sim.world.actors[0].position = [-68., 0., 10.];
        sim.submit(
            id,
            Input {
                sequence: 1,
                interact: true,
                ..Default::default()
            },
        );
        sim.step();
        assert_eq!(sim.world.jeeps[0].driver, id);
        sim.remove_actor(id);
        assert_eq!(sim.world.jeeps[0].driver, 0);
    }
    #[test]
    fn victory_resets_round_without_changing_player_ids_or_sequence_acknowledgements() {
        let mut sim = Simulation::default();
        let id = sim.add_actor(false).unwrap();
        sim.submit(
            id,
            Input {
                sequence: 42,
                ..Default::default()
            },
        );
        sim.world.tickets[1] = 0;
        sim.step();
        assert_eq!(sim.world.winner, 1);
        for _ in 0..730 {
            sim.step();
        }
        assert_eq!(sim.world.round, 2);
        assert_eq!(sim.world.winner, 0);
        assert_eq!(sim.world.tickets, [200, 200]);
        assert_eq!(sim.world.actors[0].id, id);
        assert!(!sim.submit(
            id,
            Input {
                sequence: 41,
                ..Default::default()
            }
        ));
        assert!(sim.submit(
            id,
            Input {
                sequence: 43,
                ..Default::default()
            }
        ));
    }
    #[test]
    fn bots_keep_a_ten_minute_simulated_match_finite_and_bounded() {
        let mut sim = Simulation::default();
        for _ in 0..8 {
            sim.add_actor(true);
        }
        for _ in 0..36_000 {
            sim.step();
            for actor in &sim.world.actors {
                assert!(actor.position.iter().all(|v| v.is_finite()));
                assert!(
                    actor.position[0].abs() <= MAP_LIMIT && actor.position[2].abs() <= MAP_LIMIT
                );
                assert!(actor.health <= 100 && actor.ammo <= 30);
            }
        }
    }
    #[test]
    fn malformed_datagrams_are_bounded() {
        assert!(decode::<ClientPacket>(&[255; MAX_PACKET]).is_err());
        let mut bytes = encode(&ClientPacket::Hello {
            protocol: PROTOCOL,
            nonce: 1,
        })
        .unwrap();
        bytes.push(0);
        assert!(decode::<ClientPacket>(&bytes).is_err());
    }
}
