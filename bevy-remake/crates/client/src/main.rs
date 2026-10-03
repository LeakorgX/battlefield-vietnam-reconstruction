mod network;
mod scene;
use bevy::{
    input::mouse::AccumulatedMouseMotion,
    prelude::*,
    window::{CursorGrabMode, CursorOptions},
};
use bfv_shared::*;
use network::Connection;
use std::{
    process::{Child, Command},
    time::{Duration, Instant},
};

#[derive(Resource)]
struct Settings {
    smoke: bool,
    screenshot: Option<String>,
    started: Instant,
    exit_after: Option<Duration>,
    captured: bool,
}
#[derive(Resource)]
struct HostedServer(Option<Child>);
impl Drop for HostedServer {
    fn drop(&mut self) {
        if let Some(child) = self.0.as_mut() {
            let _ = child.kill();
            let _ = child.wait();
        }
    }
}
fn main() {
    let args: Vec<String> = std::env::args().collect();
    let value = |flag: &str| args.windows(2).find(|a| a[0] == flag).map(|a| a[1].clone());
    if args.iter().any(|a| a == "--help") {
        println!(
            "bfv-client [--host] [--connect HOST:23000] [--smoke] [--exit-after SECONDS] [--screenshot PATH]\nWASD move; mouse aim; left click fire/capture cursor; Shift sprint; Space jump; R reload; E enter/exit jeep; Tab scoreboard; Esc release cursor; F10 quit."
        );
        return;
    }
    let host = args.iter().any(|a| a == "--host");
    let smoke = args.iter().any(|a| a == "--smoke");
    let child = if host {
        let exe = std::env::current_exe()
            .expect("client path")
            .with_file_name("bfv-server.exe");
        let mut command = Command::new(exe);
        command.args(["--bind", "127.0.0.1:23000"]);
        #[cfg(windows)]
        {
            use std::os::windows::process::CommandExt;
            command.creation_flags(0x08000000);
        }
        Some(
            command
                .spawn()
                .expect("Build bfv-server first, or run Start-Game.ps1"),
        )
    } else {
        None
    };
    let address = value("--connect").unwrap_or(format!("127.0.0.1:{PORT}"));
    App::new()
        .add_plugins(DefaultPlugins.set(WindowPlugin {
            primary_window: Some(Window {
                title: "Vietnam / Bevy — Conquest".into(),
                resolution: (1280, 800).into(),
                visible: !smoke,
                ..default()
            }),
            ..default()
        }))
        .insert_resource(ClearColor(Color::srgb(0.58, 0.67, 0.62)))
        .insert_resource(GlobalAmbientLight {
            color: Color::srgb(0.85, 0.9, 0.8),
            brightness: 600.,
            ..default()
        })
        .insert_resource(Time::<Fixed>::from_hz(TICK_RATE as f64))
        .insert_resource(Connection::new(&address).expect("Cannot resolve or connect UDP socket"))
        .insert_resource(HostedServer(child))
        .insert_resource(Settings {
            smoke,
            screenshot: value("--screenshot"),
            started: Instant::now(),
            exit_after: value("--exit-after")
                .map(|v| Duration::from_secs(v.parse().expect("invalid --exit-after"))),
            captured: false,
        })
        .add_plugins(scene::ScenePlugin)
        .add_systems(FixedUpdate, network::send_input)
        .add_systems(Update, (network::receive, controls, lifecycle).chain())
        .run();
}
fn controls(
    mut connection: ResMut<Connection>,
    keys: Res<ButtonInput<KeyCode>>,
    mouse: Res<ButtonInput<MouseButton>>,
    motion: Res<AccumulatedMouseMotion>,
    mut cursor: Single<&mut CursorOptions>,
    windows: Single<&Window>,
    settings: Res<Settings>,
) {
    if keys.just_pressed(KeyCode::Escape) || !windows.focused {
        cursor.grab_mode = CursorGrabMode::None;
        cursor.visible = true;
    } else if mouse.just_pressed(MouseButton::Left) && !settings.smoke {
        cursor.grab_mode = CursorGrabMode::Locked;
        cursor.visible = false;
    }
    let active = cursor.grab_mode != CursorGrabMode::None && windows.focused;
    if active {
        connection.input.yaw = (connection.input.yaw - motion.delta.x * 0.0022
            + std::f32::consts::PI)
            .rem_euclid(std::f32::consts::TAU)
            - std::f32::consts::PI;
        connection.input.pitch =
            (connection.input.pitch - motion.delta.y * 0.0022).clamp(-1.45, 1.45);
    }
    connection.input.forward = if active {
        f32::from(keys.pressed(KeyCode::KeyW)) - f32::from(keys.pressed(KeyCode::KeyS))
    } else {
        0.
    };
    connection.input.strafe = if active {
        f32::from(keys.pressed(KeyCode::KeyD)) - f32::from(keys.pressed(KeyCode::KeyA))
    } else {
        0.
    };
    connection.input.fire = active && mouse.pressed(MouseButton::Left);
    connection.input.sprint = active && keys.pressed(KeyCode::ShiftLeft);
    connection.input.jump = active && keys.pressed(KeyCode::Space);
    connection.input.reload = active && keys.pressed(KeyCode::KeyR);
    connection.input.interact = active && keys.pressed(KeyCode::KeyE);
    connection.scoreboard = keys.pressed(KeyCode::Tab);
}
fn lifecycle(
    mut commands: Commands,
    mut settings: ResMut<Settings>,
    keys: Res<ButtonInput<KeyCode>>,
    connection: Res<Connection>,
    mut exit: MessageWriter<AppExit>,
) {
    if !settings.captured
        && connection.world.is_some()
        && settings.started.elapsed() > Duration::from_secs(4)
    {
        if let Some(path) = &settings.screenshot {
            use bevy::render::view::screenshot::{Screenshot, save_to_disk};
            commands
                .spawn(Screenshot::primary_window())
                .observe(save_to_disk(path.clone()));
        }
        settings.captured = true;
    }
    if keys.just_pressed(KeyCode::F10)
        || settings
            .exit_after
            .is_some_and(|duration| settings.started.elapsed() >= duration)
    {
        exit.write(if settings.smoke && connection.world.is_none() {
            AppExit::error()
        } else {
            AppExit::Success
        });
    }
}
