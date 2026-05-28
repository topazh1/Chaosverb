// swift-tools-version:5.9
import PackageDescription

// Pure-logic core of the Studio Acoustic Planner.
// These targets are Foundation-only (no RoomPlan/ARKit/SwiftUI), so they build
// and unit-test on macOS *and* Linux. The iOS app layer (capture + UI) lives in
// the Xcode project and depends on this package.
let package = Package(
    name: "AcousticPlanner",
    products: [
        .library(name: "Acoustics", targets: ["Acoustics"]),
        .library(name: "Measurement", targets: ["Measurement"]),
    ],
    targets: [
        .target(name: "Acoustics"),
        .target(name: "Measurement", dependencies: ["Acoustics"]),
        .testTarget(name: "AcousticsTests", dependencies: ["Acoustics"]),
        .testTarget(name: "MeasurementTests", dependencies: ["Measurement"]),
    ]
)
