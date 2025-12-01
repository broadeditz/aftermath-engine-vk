const std = @import("std");

// Although this function looks imperative, it does not perform the build
// directly and instead it mutates the build graph (`b`) that will be then
// executed by an external runner. The functions in `std.Build` implement a DSL
// for defining build steps and express dependencies between them, allowing the
// build runner to parallelize the build automatically (and the cache system to
// know when a step doesn't need to be re-run).
pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{ .name = "Aftermath", .root_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    }) });

    // Add C++ source files
    exe.addCSourceFiles(.{
        .files = &.{
            "src/main.cpp",

            "src/camera/camera.cpp",

            "src/screen/computescreen.cpp",

            "src/tree/tree.cpp",
            "src/tree/tree_stale.cpp",
            "src/tree/tree_util.cpp",

            "src/uniforms/frame.cpp",
            "src/uniforms/render.cpp",
        },
        .flags = &.{
            "-std=c++23",
            "-Wall",
            "-Wextra",
            "-DVULKAN_HPP_NO_STRUCT_CONSTRUCTORS",
            "-DVULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1",
            "-Wno-nullability-completeness",
            "-Wno-nullability-extension",
            "-Wno-unused-private-field",
            "-Wno-unknown-pragmas",
        },
    });

    // Link C++ standard library
    exe.linkLibCpp();

    // Vulkan SDK (from environment variable or hardcoded path)
    const vulkan_sdk = std.process.getEnvVarOwned(b.allocator, "VULKAN_SDK") catch null;

    if (vulkan_sdk) |sdk| {
        const vulkan_lib = b.fmt("{s}/Lib", .{sdk});
        const vulkan_include = b.fmt("{s}/Include", .{sdk});

        exe.addLibraryPath(.{ .cwd_relative = vulkan_lib });
        exe.addIncludePath(.{ .cwd_relative = vulkan_include });
    } else {
        // Fallback to hardcoded path if VULKAN_SDK not set
        switch (target.result.os.tag) {
            .windows => {
                exe.addLibraryPath(.{ .cwd_relative = "C:/VulkanSDK/1.4.XXX.X/Lib" });
                exe.addIncludePath(.{ .cwd_relative = "C:/VulkanSDK/1.4.XXX.X/Include" });
            },
            .linux => {
                exe.addLibraryPath(.{ .cwd_relative = "/usr/lib" });
                exe.addIncludePath(.{ .cwd_relative = "/usr/include" });
            },
            .macos => {
                exe.addLibraryPath(.{ .cwd_relative = "/usr/local/lib" });
                exe.addIncludePath(.{ .cwd_relative = "/usr/local/include" });
            },
            else => {
                @panic("Unsupported OS - please set VULKAN_SDK environment variable");
            },
        }
    }

    const vulkan_lib_name = if (target.result.os.tag == .windows) "vulkan-1" else "vulkan";
    exe.linkSystemLibrary(vulkan_lib_name);

    // Build vcpkg triplet based on target
    const vcpkg_triplet = getVcpkgTriplet(b, target.result);
    const vcpkg_path = b.fmt("vcpkg_installed/{s}", .{vcpkg_triplet});

    exe.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/lib", .{vcpkg_path}) });
    exe.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/include", .{vcpkg_path}) });
    exe.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/bin", .{vcpkg_path}) });

    if (target.result.os.tag == .windows) {
        const install_dll = b.addInstallFile(.{ .cwd_relative = b.fmt("vcpkg_installed/{s}/bin/glfw3.dll", .{vcpkg_triplet}) }, "bin/glfw3.dll");

        install_dll.step.dependOn(&exe.step);
        b.getInstallStep().dependOn(&install_dll.step);
    }

    exe.linkSystemLibrary("glfw3");
    exe.linkSystemLibrary("glm");

    exe.addIncludePath(.{ .cwd_relative = b.fmt("{s}/include", .{vcpkg_path}) });

    // Shader compilation
    const slangc_path = if (vulkan_sdk) |sdk|
        switch (target.result.os.tag) {
            .windows => b.fmt("{s}/Bin/slangc.exe", .{sdk}),
            else => b.fmt("{s}/bin/slangc", .{sdk}),
        }
    else switch (target.result.os.tag) {
        .windows => "C:\\VulkanSDK\\1.4.XXX.X\\Bin\\slangc.exe",
        .linux => "/usr/bin/slangc",
        .macos => "/usr/local/bin/slangc",
        else => "slangc",
    };

    // Verify slangc binary exists
    std.fs.cwd().access(slangc_path, .{}) catch |err| {
        std.debug.print("Warning: slangc not found at {s}: {}\n", .{ slangc_path, err });
    };

    const compile_shaders = b.step("shaders", "Compile Slang shaders");

    const shader_output_path = b.pathJoin(&.{ b.install_prefix, "bin", "shaders", "slang.spv" });

    const shader_cmd = b.addSystemCommand(&.{slangc_path});
    shader_cmd.addArgs(&.{
        "src/shaders/shader.slang",
        "-target",
        "spirv",
        "-profile",
        "spirv_1_4",
        "-emit-spirv-directly",
        "-fvk-use-entrypoint-name",
        "-entry",
        "vertMain",
        "-entry",
        "fragMain",
        "-entry",
        "computeMain",
        "-o",
        shader_output_path,
    });

    // Ensure output directory exists before running command
    const ensure_dir = b.addSystemCommand(&.{ "cmd", "/c", "if not exist" });
    ensure_dir.addArg(b.pathJoin(&.{ b.install_prefix, "bin", "shaders" }));
    ensure_dir.addArg("mkdir");
    ensure_dir.addArg(b.pathJoin(&.{ b.install_prefix, "bin", "shaders" }));

    shader_cmd.step.dependOn(&ensure_dir.step);
    compile_shaders.dependOn(&shader_cmd.step);
    b.getInstallStep().dependOn(compile_shaders);

    // Install the executable
    b.installArtifact(exe);

    // Create a run step
    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    run_cmd.setCwd(.{ .cwd_relative = "zig-out/bin" });

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the application");
    run_step.dependOn(&run_cmd.step);

    // Generate compile_commands.json
    generateCompileCommands(b, target) catch |err| {
        std.debug.print("Failed to generate compile_commands.json: {}\n", .{err});
    };
}

fn getVcpkgTriplet(b: *std.Build, target: std.Target) []const u8 {
    const arch = switch (target.cpu.arch) {
        .x86_64 => "x64",
        .x86 => "x86",
        .aarch64 => "arm64",
        .arm => "arm",
        else => "unknown",
    };

    const os = switch (target.os.tag) {
        .linux => "linux",
        .windows => "windows",
        .macos => "osx",
        else => "unknown",
    };

    return b.fmt("{s}-{s}", .{ arch, os });
}

fn generateCompileCommands(b: *std.Build, target: std.Build.ResolvedTarget) !void {
    const vcpkg_triplet = getVcpkgTriplet(b, target.result);
    const vcpkg_include = b.fmt("vcpkg_installed/{s}/include", .{vcpkg_triplet});

    const vulkan_sdk = std.process.getEnvVarOwned(b.allocator, "VULKAN_SDK") catch null;
    const vulkan_include = if (vulkan_sdk) |sdk|
        b.fmt("{s}/Include", .{sdk})
    else switch (target.result.os.tag) {
        // TODO: find installed vulkan version
        .windows => try findLatestVulkanVersion(b.allocator),
        .linux => "/usr/include",
        .macos => "/usr/local/include",
        else => "/usr/include",
    };

    // Normalize paths to use forward slashes
    const vcpkg_include_normalized = try normalizePathSeparators(b.allocator, vcpkg_include);
    const vulkan_include_normalized = try normalizePathSeparators(b.allocator, vulkan_include);
    const root = b.build_root.path orelse ".";
    const root_normalized = try normalizePathSeparators(b.allocator, root);

    const sources = [_][]const u8{
        "src/main.cpp",
        "src/screen/computescreen.cpp",
        "src/uniforms/frame.cpp",
        "src/uniforms/render.cpp",
        "src/tree/tree.cpp",
        "src/tree/tree_stale.cpp",
        "src/tree/tree_util.cpp",
        "src/camera/camera.cpp",
    };

    var compile_commands = try std.ArrayList(u8).initCapacity(b.allocator, 4096);
    const writer = compile_commands.writer(b.allocator);

    try writer.writeAll("[\n");

    for (sources, 0..) |source, i| {
        try writer.print(
            \\  {{
            \\    "directory": "{s}",
            \\    "command": "clang++ -std=c++23 -Wall -Wextra -DVULKAN_HPP_NO_STRUCT_CONSTRUCTORS -DVULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 -Wno-nullability-completeness -Wno-nullability-extension -Wno-unused-private-field -Wno-unknown-pragmas -I\"{s}\" -I\"{s}\" -c \"{s}\"",
            \\    "file": "{s}"
            \\  }}
        , .{ root_normalized, vcpkg_include_normalized, vulkan_include_normalized, source, source });

        if (i < sources.len - 1) {
            try writer.writeAll(",\n");
        } else {
            try writer.writeAll("\n");
        }
    }

    try writer.writeAll("]\n");

    const file = try std.fs.cwd().createFile("compile_commands.json", .{});
    defer file.close();
    try file.writeAll(compile_commands.items);
}

fn findLatestVulkanVersion(allocator: std.mem.Allocator) ![]const u8 {
    const vulkan_sdk_base = "C:/VulkanSDK";

    var dir = std.fs.openDirAbsolute(vulkan_sdk_base, .{ .iterate = true }) catch |err| {
        std.log.err("Failed to open VulkanSDK directory at {s}: {}", .{ vulkan_sdk_base, err });
        return error.VulkanSDKNotFound;
    };
    defer dir.close();

    var latest_version: ?struct { major: u32, minor: u32, patch: u32, build: u32, path: []const u8 } = null;

    var iter = dir.iterate();
    while (try iter.next()) |entry| {
        if (entry.kind != .directory) continue;

        // Parse version string (e.g., "1.4.328.1")
        var parts = std.mem.splitScalar(u8, entry.name, '.');
        const major = std.fmt.parseInt(u32, parts.next() orelse continue, 10) catch continue;
        const minor = std.fmt.parseInt(u32, parts.next() orelse continue, 10) catch continue;
        const patch = std.fmt.parseInt(u32, parts.next() orelse continue, 10) catch continue;
        const b = std.fmt.parseInt(u32, parts.next() orelse continue, 10) catch continue;

        // Only consider 1.4.x versions
        if (major != 1 or minor != 4) continue;

        if (latest_version == null or
            patch > latest_version.?.patch or
            (patch == latest_version.?.patch and b > latest_version.?.build))
        {
            const path = try std.fmt.allocPrint(allocator, "{s}/{s}/Include", .{ vulkan_sdk_base, entry.name });
            latest_version = .{ .major = major, .minor = minor, .patch = patch, .build = b, .path = path };
        }
    }

    if (latest_version) |version| {
        std.log.info("Found Vulkan SDK 1.4.{d}.{d} at {s}", .{ version.patch, version.build, version.path });
        return version.path;
    }

    std.log.err("No Vulkan SDK 1.4.x installation found in {s}", .{vulkan_sdk_base});
    return error.NoVulkan14SDKFound;
}

fn normalizePathSeparators(allocator: std.mem.Allocator, path: []const u8) ![]u8 {
    const normalized = try allocator.alloc(u8, path.len);
    for (path, 0..) |c, i| {
        normalized[i] = if (c == '\\') '/' else c;
    }
    return normalized;
}
