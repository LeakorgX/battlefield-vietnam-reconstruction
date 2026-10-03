use crate::network::Connection;
use bevy::{prelude::*, text::FontSize};
use bfv_shared::*;
use std::collections::HashSet;

pub struct ScenePlugin;
impl Plugin for ScenePlugin {
    fn build(&self, app: &mut App) {
        app.add_systems(Startup, setup).add_systems(
            Update,
            (sync_entities, update_camera, update_hud)
                .chain()
                .after(crate::controls)
                .before(crate::lifecycle),
        );
    }
}

#[derive(Component)]
struct Soldier(u32);
#[derive(Component)]
struct JeepVisual(usize);
#[derive(Component)]
struct FlagVisual(usize);
#[derive(Component)]
struct PlayerCamera;
#[derive(Component)]
struct Hud;
#[derive(Component)]
struct Tickets;
#[derive(Component)]
struct Objectives;
#[derive(Component)]
struct Scoreboard;
#[derive(Component)]
struct Hint;
#[derive(Component)]
struct MapDot(u32);
#[derive(Component)]
struct Weapon;
#[derive(Resource)]
struct Art {
    cube: Handle<Mesh>,
    body: Handle<Mesh>,
    head: Handle<Mesh>,
    teams: [Handle<StandardMaterial>; 2],
    neutral: Handle<StandardMaterial>,
    dark: Handle<StandardMaterial>,
}
fn cube(
    commands: &mut Commands,
    mesh: &Handle<Mesh>,
    material: &Handle<StandardMaterial>,
    center: Vec3,
    size: Vec3,
) -> Entity {
    commands
        .spawn((
            Mesh3d(mesh.clone()),
            MeshMaterial3d(material.clone()),
            Transform::from_translation(center).with_scale(size),
        ))
        .id()
}
fn setup(
    mut commands: Commands,
    mut meshes: ResMut<Assets<Mesh>>,
    mut materials: ResMut<Assets<StandardMaterial>>,
) {
    let cube_mesh = meshes.add(Cuboid::new(1., 1., 1.));
    let ground = materials.add(Color::srgb(0.24, 0.32, 0.17));
    let dirt = materials.add(Color::srgb(0.42, 0.35, 0.22));
    let bamboo = materials.add(Color::srgb(0.38, 0.3, 0.16));
    let roof = materials.add(Color::srgb(0.21, 0.24, 0.15));
    let dark = materials.add(Color::srgb(0.08, 0.1, 0.08));
    let leaves = materials.add(Color::srgb(0.14, 0.28, 0.12));
    let blue = materials.add(Color::srgb(0.16, 0.51, 0.8));
    let red = materials.add(Color::srgb(0.85, 0.24, 0.16));
    let neutral = materials.add(Color::srgb(0.8, 0.75, 0.57));
    cube(
        &mut commands,
        &cube_mesh,
        &ground,
        Vec3::new(0., -0.3, 0.),
        Vec3::new(240., 0.6, 240.),
    );
    cube(
        &mut commands,
        &cube_mesh,
        &dirt,
        Vec3::new(0., 0.012, 0.),
        Vec3::new(170., 0.02, 7.),
    );
    cube(
        &mut commands,
        &cube_mesh,
        &dirt,
        Vec3::new(0., 0.012, 0.),
        Vec3::new(7., 0.02, 130.),
    );
    for b in BLOCKS {
        cube(
            &mut commands,
            &cube_mesh,
            &bamboo,
            Vec3::from_array(b.center),
            Vec3::from_array(b.half) * 2.,
        );
        if b.half[1] >= 1.5 {
            cube(
                &mut commands,
                &cube_mesh,
                &roof,
                Vec3::new(b.center[0], b.center[1] + b.half[1] + 0.2, b.center[2]),
                Vec3::new(b.half[0] * 2. + 1., 0.4, b.half[2] * 2. + 1.),
            );
            cube(
                &mut commands,
                &cube_mesh,
                &dark,
                Vec3::new(b.center[0], 1., b.center[2] + b.half[2] + 0.025),
                Vec3::new(1.5, 2., 0.03),
            );
        }
    }
    // Decorative foliage stays outside gameplay routes; colliders are defined in shared::BLOCKS.
    let trunk_mesh = meshes.add(Cylinder::new(0.28, 6.));
    let canopy_mesh = meshes.add(Sphere::new(1.));
    for i in 0..180 {
        let x = ((i * 47 % 211) as f32 - 105.) + 0.3;
        let z = ((i * 83 % 211) as f32 - 105.) + 0.3;
        if x.abs() < 82. && z.abs() < 50. {
            continue;
        }
        commands.spawn((
            Mesh3d(trunk_mesh.clone()),
            MeshMaterial3d(bamboo.clone()),
            Transform::from_xyz(x, 3., z),
        ));
        commands.spawn((
            Mesh3d(canopy_mesh.clone()),
            MeshMaterial3d(leaves.clone()),
            Transform::from_xyz(x, 6., z).with_scale(Vec3::new(3., 1.6, 3.)),
        ));
    }
    for (i, p) in FLAG_POSITIONS.iter().enumerate() {
        cube(
            &mut commands,
            &cube_mesh,
            &dark,
            Vec3::new(p[0], 3., p[2]),
            Vec3::new(0.13, 6., 0.13),
        );
        let entity = cube(
            &mut commands,
            &cube_mesh,
            &neutral,
            Vec3::new(p[0] + 0.9, 5.3, p[2]),
            Vec3::new(1.8, 1., 0.06),
        );
        commands.entity(entity).insert(FlagVisual(i));
        cube(
            &mut commands,
            &cube_mesh,
            &dirt,
            Vec3::new(p[0], 0.018, p[2]),
            Vec3::new(17., 0.025, 17.),
        );
    }
    for (i, p) in [[-68., 0., 12.], [68., 0., 12.]].iter().enumerate() {
        commands
            .spawn((
                JeepVisual(i),
                Transform::from_translation(Vec3::from_array(*p)),
                Visibility::default(),
            ))
            .with_children(|parent| {
                parent.spawn((
                    Mesh3d(cube_mesh.clone()),
                    MeshMaterial3d(roof.clone()),
                    Transform::from_xyz(0., 0.75, 0.).with_scale(Vec3::new(1.7, 0.55, 3.1)),
                ));
                parent.spawn((
                    Mesh3d(cube_mesh.clone()),
                    MeshMaterial3d(dark.clone()),
                    Transform::from_xyz(0., 1., 0.25).with_scale(Vec3::new(1.3, 0.3, 1.)),
                ));
                for x in [-0.95, 0.95] {
                    for z in [-1., 1.] {
                        parent.spawn((
                            Mesh3d(cube_mesh.clone()),
                            MeshMaterial3d(dark.clone()),
                            Transform::from_xyz(x, 0.45, z).with_scale(Vec3::new(0.35, 0.8, 0.8)),
                        ));
                    }
                }
            });
    }
    commands.spawn((
        DirectionalLight {
            illuminance: 14000.,
            shadow_maps_enabled: true,
            ..default()
        },
        Transform::from_rotation(Quat::from_euler(EulerRot::XYZ, -0.85, -0.6, 0.)),
    ));
    commands
        .spawn((
            Camera3d::default(),
            PlayerCamera,
            Transform::from_xyz(-78., 1.65, 0.).looking_at(Vec3::ZERO, Vec3::Y),
            DistanceFog {
                color: Color::srgb(0.58, 0.67, 0.62),
                falloff: FogFalloff::Linear {
                    start: 65.,
                    end: 190.,
                },
                ..default()
            },
        ))
        .with_children(|parent| {
            parent
                .spawn((Weapon, Transform::default(), Visibility::default()))
                .with_children(|weapon| {
                    weapon.spawn((
                        Mesh3d(cube_mesh.clone()),
                        MeshMaterial3d(dark.clone()),
                        Transform::from_xyz(0.28, -0.27, -0.55)
                            .with_scale(Vec3::new(0.1, 0.13, 0.7)),
                    ));
                    weapon.spawn((
                        Mesh3d(cube_mesh.clone()),
                        MeshMaterial3d(bamboo.clone()),
                        Transform::from_xyz(0.28, -0.32, -0.3)
                            .with_scale(Vec3::new(0.12, 0.1, 0.4)),
                    ));
                });
        });
    commands.insert_resource(Art {
        cube: cube_mesh,
        body: meshes.add(Capsule3d::new(0.35, 0.9)),
        head: meshes.add(Sphere::new(0.22)),
        teams: [blue, red],
        neutral,
        dark,
    });
    setup_ui(&mut commands);
}
fn font(size: f32) -> TextFont {
    TextFont {
        font_size: FontSize::Px(size),
        ..default()
    }
}
fn setup_ui(commands: &mut Commands) {
    let ink = TextColor(Color::srgb(0.93, 0.92, 0.81));
    let panel = BackgroundColor(Color::srgba(0.045, 0.065, 0.045, 0.9));
    commands
        .spawn((
            Node {
                position_type: PositionType::Absolute,
                left: px(24.),
                top: px(20.),
                padding: UiRect::all(px(14.)),
                ..default()
            },
            panel,
        ))
        .with_child((
            Text::new("VIETNAM / BEVY\nCONQUEST / RIVER VILLAGE"),
            font(20.),
            ink,
        ));
    commands.spawn((
        Tickets,
        Text::new("US  200     /     NVA  200"),
        font(28.),
        ink,
        Node {
            position_type: PositionType::Absolute,
            right: px(24.),
            top: px(24.),
            ..default()
        },
    ));
    commands.spawn((
        Objectives,
        Text::new("A  RIVER OUTPOST     B  VILLAGE     C  HILL CAMP"),
        font(17.),
        ink,
        Node {
            position_type: PositionType::Absolute,
            top: px(100.),
            left: percent(22.),
            ..default()
        },
    ));
    commands
        .spawn((
            Node {
                position_type: PositionType::Absolute,
                left: px(24.),
                bottom: px(24.),
                padding: UiRect::all(px(14.)),
                ..default()
            },
            panel,
        ))
        .with_child((Hud, Text::new("CONNECTING..."), font(20.), ink));
    commands.spawn((
        Hint,
        Text::new("Click to aim | WASD move | R reload | E jeep | Tab scores | F10 quit"),
        font(16.),
        ink,
        Node {
            position_type: PositionType::Absolute,
            left: px(24.),
            bottom: px(140.),
            ..default()
        },
    ));
    commands.spawn((
        Text::new("+"),
        font(25.),
        ink,
        Node {
            position_type: PositionType::Absolute,
            left: percent(49.5),
            top: percent(48.),
            ..default()
        },
    ));
    commands.spawn((
        Scoreboard,
        Text::new(""),
        font(20.),
        ink,
        Node {
            position_type: PositionType::Absolute,
            left: percent(30.),
            top: percent(25.),
            padding: UiRect::all(px(24.)),
            ..default()
        },
        panel,
        Visibility::Hidden,
    ));
    commands
        .spawn((
            Node {
                position_type: PositionType::Absolute,
                right: px(24.),
                bottom: px(24.),
                width: px(172.),
                height: px(172.),
                ..default()
            },
            panel,
        ))
        .with_children(|parent| {
            parent.spawn((
                Text::new("N ^  /  SECTOR"),
                font(12.),
                ink,
                Node {
                    position_type: PositionType::Absolute,
                    top: px(3.),
                    left: px(5.),
                    ..default()
                },
            ));
            for (i, p) in FLAG_POSITIONS.iter().enumerate() {
                parent.spawn((
                    Text::new(["A", "B", "C"][i]),
                    font(14.),
                    ink,
                    Node {
                        position_type: PositionType::Absolute,
                        left: px(86. + p[0] * 0.75),
                        top: px(86. + p[2] * 0.75),
                        ..default()
                    },
                ));
            }
            // Stable slots let snapshots change without rebuilding the minimap hierarchy.
            for slot in 0..MAX_ACTORS {
                parent.spawn((
                    MapDot(slot as u32),
                    Node {
                        position_type: PositionType::Absolute,
                        width: px(5.),
                        height: px(5.),
                        ..default()
                    },
                    BackgroundColor(Color::WHITE),
                    Visibility::Hidden,
                ));
            }
        });
}
fn sync_entities(
    mut commands: Commands,
    connection: Res<Connection>,
    art: Res<Art>,
    mut soldiers: Query<(Entity, &Soldier, &mut Transform, &mut Visibility)>,
    mut flags: Query<(&FlagVisual, &mut MeshMaterial3d<StandardMaterial>)>,
    mut jeeps: Query<(&JeepVisual, &mut Transform), Without<Soldier>>,
    mut gizmos: Gizmos,
) {
    let Some(world) = &connection.world else {
        return;
    };
    let alpha = (connection.received.elapsed().as_secs_f32() * 20.).clamp(0., 1.);
    let mut existing = HashSet::new();
    for (entity, soldier, mut transform, mut visibility) in &mut soldiers {
        if let Some(a) = world.actors.iter().find(|a| a.id == soldier.0) {
            existing.insert(a.id);
            *visibility = if a.id == connection.id || a.health == 0 {
                Visibility::Hidden
            } else {
                Visibility::Visible
            };
            let current = Vec3::from_array(a.position);
            let previous = connection
                .previous
                .as_ref()
                .and_then(|w| w.actors.iter().find(|b| b.id == a.id))
                .map(|b| Vec3::from_array(b.position))
                .unwrap_or(current);
            transform.translation = if previous.distance(current) > 10. {
                current
            } else {
                previous.lerp(current, alpha)
            };
            transform.rotation = Quat::from_rotation_y(a.yaw);
        } else {
            commands.entity(entity).despawn();
        }
    }
    for a in &world.actors {
        if existing.contains(&a.id) {
            continue;
        }
        commands
            .spawn((
                Soldier(a.id),
                Transform::from_translation(Vec3::from_array(a.position)),
                Visibility::Hidden,
            ))
            .with_children(|parent| {
                parent.spawn((
                    Mesh3d(art.body.clone()),
                    MeshMaterial3d(art.teams[(a.team - 1) as usize].clone()),
                    Transform::from_xyz(0., 0.8, 0.),
                ));
                parent.spawn((
                    Mesh3d(art.head.clone()),
                    MeshMaterial3d(art.teams[(a.team - 1) as usize].clone()),
                    Transform::from_xyz(0., 1.6, 0.),
                ));
                parent.spawn((
                    Mesh3d(art.cube.clone()),
                    MeshMaterial3d(art.dark.clone()),
                    Transform::from_xyz(0.25, 1.15, -0.3).with_scale(Vec3::new(0.12, 0.12, 0.7)),
                ));
            });
    }
    for (flag, mut material) in &mut flags {
        material.0 = match world.flags[flag.0].owner {
            1 => art.teams[0].clone(),
            2 => art.teams[1].clone(),
            _ => art.neutral.clone(),
        };
    }
    for (index, mut transform) in &mut jeeps {
        let jeep = &world.jeeps[index.0];
        transform.translation = Vec3::from_array(jeep.position);
        transform.rotation = Quat::from_rotation_y(jeep.yaw);
    }
    for (i, p) in FLAG_POSITIONS.iter().enumerate() {
        let color = match world.flags[i].owner {
            1 => Color::srgb(0.2, 0.6, 1.),
            2 => Color::srgb(1., 0.3, 0.2),
            _ => Color::srgb(0.9, 0.8, 0.5),
        };
        gizmos.circle(
            Isometry3d::new(
                Vec3::new(p[0], 0.06, p[2]),
                Quat::from_rotation_x(std::f32::consts::FRAC_PI_2),
            ),
            9.,
            color,
        );
    }
    if let Some(previous) = &connection.previous {
        for a in &world.actors {
            if a.health == 0
                || !previous
                    .actors
                    .iter()
                    .any(|b| b.id == a.id && b.shot != a.shot)
                || connection.received.elapsed().as_secs_f32() > 0.05
            {
                continue;
            }
            let start = Vec3::from_array(a.position) + Vec3::Y * 1.65;
            let dir = direction(a.yaw, a.pitch);
            let length = BLOCKS
                .iter()
                .filter_map(|b| ray_box(start.to_array(), dir, b.center, b.half))
                .fold(50., f32::min);
            gizmos.line(
                start,
                start + Vec3::from_array(dir) * length,
                Color::srgb(1., 0.85, 0.35),
            );
        }
    }
}
fn update_camera(
    connection: Res<Connection>,
    mut camera: Single<&mut Transform, With<PlayerCamera>>,
    mut weapon: Single<&mut Visibility, With<Weapon>>,
) {
    let Some(world) = &connection.world else {
        return;
    };
    let Some(a) = world.actors.iter().find(|a| a.id == connection.id) else {
        return;
    };
    camera.translation =
        Vec3::from_array(connection.predicted) + Vec3::Y * if a.health > 0 { 1.65 } else { 4. };
    camera.rotation = Quat::from_euler(
        EulerRot::YXZ,
        connection.input.yaw,
        connection.input.pitch,
        0.,
    );
    **weapon = if a.vehicle < 0 && a.health > 0 {
        Visibility::Visible
    } else {
        Visibility::Hidden
    };
}
// The exclusion filters make the mutable UI queries provably disjoint for Bevy.
#[allow(clippy::type_complexity)]
fn update_hud(
    connection: Res<Connection>,
    mut hud: Single<
        &mut Text,
        (
            With<Hud>,
            Without<Tickets>,
            Without<Objectives>,
            Without<Scoreboard>,
            Without<Hint>,
        ),
    >,
    mut tickets: Single<
        &mut Text,
        (
            With<Tickets>,
            Without<Hud>,
            Without<Objectives>,
            Without<Scoreboard>,
            Without<Hint>,
        ),
    >,
    mut objectives: Single<
        &mut Text,
        (
            With<Objectives>,
            Without<Hud>,
            Without<Tickets>,
            Without<Scoreboard>,
            Without<Hint>,
        ),
    >,
    mut scoreboard: Single<
        (&mut Text, &mut Visibility),
        (
            With<Scoreboard>,
            Without<Hud>,
            Without<Tickets>,
            Without<Objectives>,
            Without<Hint>,
            Without<MapDot>,
        ),
    >,
    mut hint: Single<
        &mut Text,
        (
            With<Hint>,
            Without<Hud>,
            Without<Tickets>,
            Without<Objectives>,
            Without<Scoreboard>,
        ),
    >,
    mut dots: Query<
        (&MapDot, &mut Node, &mut BackgroundColor, &mut Visibility),
        Without<Scoreboard>,
    >,
) {
    let Some(world) = &connection.world else {
        hud.0 = format!("{}\n{}", connection.status, connection.address);
        return;
    };
    let Some(a) = world.actors.iter().find(|a| a.id == connection.id) else {
        return;
    };
    hud.0 = format!(
        "{}  /  PLAYER {:02}\nHEALTH {:03}   AMMO {:02}/30{}\nKILLS {}   DEATHS {}   ROUND {}",
        if a.team == 1 { "US" } else { "NVA" },
        a.id,
        a.health,
        a.ammo,
        if a.reloading { "  RELOADING" } else { "" },
        a.kills,
        a.deaths,
        world.round
    );
    tickets.0 = format!(
        "US  {:03}     /     NVA  {:03}",
        world.tickets[0], world.tickets[1]
    );
    objectives.0 = world
        .flags
        .iter()
        .enumerate()
        .map(|(i, f)| {
            format!(
                "{}  {} {}",
                ["A", "B", "C"][i],
                ["NEUTRAL", "US", "NVA"][f.owner as usize],
                if f.contested {
                    "CONTESTED".into()
                } else {
                    format!("{:02}%", (f.progress.abs() * 100.) as u8)
                }
            )
        })
        .collect::<Vec<_>>()
        .join("     /     ");
    let (score_text, visible) = &mut *scoreboard;
    **visible = if connection.scoreboard {
        Visibility::Visible
    } else {
        Visibility::Hidden
    };
    if connection.scoreboard {
        score_text.0 = "CONQUEST / SCOREBOARD\nTEAM   PLAYER      KILLS   DEATHS\n".into();
        for a in &world.actors {
            score_text.0.push_str(&format!(
                "{}     {:02} {}      {:02}       {:02}\n",
                if a.team == 1 { "US " } else { "NVA" },
                a.id,
                if a.bot { "BOT" } else { "   " },
                a.kills,
                a.deaths
            ));
        }
    }
    hint.0 = if world.winner != 0 {
        format!(
            "{} WINS - next round in a few seconds",
            if world.winner == 1 { "US" } else { "NVA" }
        )
    } else if a.health == 0 {
        "KILLED IN ACTION - respawning at base...".into()
    } else if a.vehicle >= 0 {
        "JEEP / W-S throttle | A-D steer | E exit".into()
    } else if let Some((i, _)) = FLAG_POSITIONS
        .iter()
        .enumerate()
        .find(|(_, p)| horizontal_distance(a.position, **p) < 9.)
    {
        format!("{}  /  Hold this area to capture", FLAG_NAMES[i])
    } else {
        "Click to aim | WASD move | R reload | E jeep | Tab scores | F10 quit".into()
    };
    for (dot, mut node, mut color, mut visibility) in &mut dots {
        if let Some(actor) = world.actors.get(dot.0 as usize).filter(|a| a.health > 0) {
            node.left = px(86. + actor.position[0] * 0.75);
            node.top = px(86. + actor.position[2] * 0.75);
            color.0 = if actor.id == connection.id {
                Color::WHITE
            } else if actor.team == 1 {
                Color::srgb(0.2, 0.6, 1.)
            } else {
                Color::srgb(1., 0.3, 0.2)
            };
            *visibility = Visibility::Visible;
        } else {
            *visibility = Visibility::Hidden;
        }
    }
}
