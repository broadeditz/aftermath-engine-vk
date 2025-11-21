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
    // const exe = b.addExecutable(.{
    //     .name = "Aftermath",
    //     .target = target,
    //     .optimize = optimize,
    // });

    // Add C++ source files
    exe.addCSourceFiles(.{
        .files = &.{
            "src/main.cpp",

            "src/camera/camera.cpp",

            "src/screen/computescreen.cpp",

            "src/tree/tree.cpp",

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

    const install_dll = b.addInstallFile(.{ .cwd_relative = b.fmt("vcpkg_installed/{s}/bin/glfw3.dll", .{vcpkg_triplet}) }, "bin/glfw3.dll");

    install_dll.step.dependOn(&exe.step);
    b.getInstallStep().dependOn(&install_dll.step);

    exe.linkSystemLibrary("glfw3");
    exe.linkSystemLibrary("glm");

    exe.addIncludePath(.{ .cwd_relative = b.fmt("{s}/include", .{vcpkg_path}) });
    exe.addIncludePath(.{ .cwd_relative = b.fmt("{s}/include/vma", .{vcpkg_path}) });

    // Install the executable
    b.installArtifact(exe);

    // Create a run step
    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the application");
    run_step.dependOn(&run_cmd.step);
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
