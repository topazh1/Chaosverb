import Foundation

public enum RecPriority: Int, Comparable {
    case low = 1, medium = 2, high = 3
    public static func < (l: RecPriority, r: RecPriority) -> Bool { l.rawValue < r.rawValue }
}

public enum RecCategory: String {
    case placement, sbir, reflection, bassTrap, absorption, mode
}

/// A single actionable recommendation. `location` (when present) drives an AR marker.
public struct RecommendationItem: Equatable {
    public let category: RecCategory
    public let priority: RecPriority
    public let title: String
    public let detail: String
    public let location: Point3?
    public init(category: RecCategory, priority: RecPriority, title: String, detail: String, location: Point3? = nil) {
        self.category = category; self.priority = priority
        self.title = title; self.detail = detail; self.location = location
    }
}

public struct RecommendationReport: Equatable {
    public let setup: ListeningSetup
    public let problemModes: [RoomMode]
    public let rt60: Double               // representative (500 Hz band)
    public let absorptionToAddM2: Double  // broadband m² needed to hit target
    public let items: [RecommendationItem]
    public let isMeasured: Bool

    public init(setup: ListeningSetup, problemModes: [RoomMode], rt60: Double,
                absorptionToAddM2: Double, items: [RecommendationItem], isMeasured: Bool) {
        self.setup = setup
        self.problemModes = problemModes
        self.rt60 = rt60
        self.absorptionToAddM2 = absorptionToAddM2
        self.items = items
        self.isMeasured = isMeasured
    }
}

public enum RecommendationEngine {

    /// Generate a prediction-only report. (Measurement refinement lives in the
    /// `Measurement` module's `MeasuredRefinement`.)
    public static func generate(
        room: RoomDimensions,
        tags: MaterialTags,
        monitorFrontDistance: Double = 0.5,
        earHeight: Double = 1.2,
        targetRT60: Double = 0.30
    ) -> RecommendationReport {
        let setup = Placement.recommend(room: room, monitorFrontDistance: monitorFrontDistance, earHeight: earHeight)
        let modes = RoomModes.calculate(room)
        var items: [RecommendationItem] = []

        // 1) Listening position + monitors
        items.append(RecommendationItem(
            category: .placement, priority: .high,
            title: "Set up the listening triangle",
            detail: String(format: "Sit %.2f m from the front wall (38%% rule). Space the monitors %.2f m apart at %.2f m ear height, toed in 30° toward your head.",
                           setup.listener.x, setup.monitorSpacing, setup.earHeight),
            location: setup.listener))
        if !setup.fits(in: room) {
            items.append(RecommendationItem(
                category: .placement, priority: .medium,
                title: "Room is narrow for this triangle",
                detail: "The ideal monitor spacing is wider than the room comfortably allows — reduce listening distance or accept tighter spacing."))
        }

        // 2) SBIR — monitor distance from the front wall
        let null = SBIR.firstNull(distance: monitorFrontDistance)
        if SBIR.nullIsProblematic(distance: monitorFrontDistance) {
            items.append(RecommendationItem(
                category: .sbir, priority: .high,
                title: "Boundary cancellation in the bass",
                detail: String(format: "At %.2f m from the front wall you'll get an SBIR null near %.0f Hz. Either push the monitors closer to the wall (<0.43 m) or treat the front wall behind them heavily.",
                               monitorFrontDistance, null)))
        }

        // 3) First reflections — side walls (near-side monitor) + ceiling
        let leftPt = Reflections.reflectionPoint(source: setup.leftMonitor, listener: setup.listener, axis: .y, planeCoord: 0)
        let rightPt = Reflections.reflectionPoint(source: setup.rightMonitor, listener: setup.listener, axis: .y, planeCoord: room.width)
        items.append(RecommendationItem(
            category: .reflection, priority: .high,
            title: "Treat the left-wall first reflection",
            detail: "Mount a broadband panel (≈60×120 cm) centred here to kill the side-wall reflection.",
            location: leftPt))
        items.append(RecommendationItem(
            category: .reflection, priority: .high,
            title: "Treat the right-wall first reflection",
            detail: "Mount a broadband panel (≈60×120 cm) centred here (mirror of the left).",
            location: rightPt))
        let ceilPt = Reflections.reflectionPoint(source: setup.leftMonitor, listener: setup.listener, axis: .z, planeCoord: room.height)
        items.append(RecommendationItem(
            category: .reflection, priority: .medium,
            title: "Add a ceiling cloud",
            detail: "Hang an absorptive cloud above the mix position, centred over the reflection point.",
            location: ceilPt))

        // 4) Bass traps — the four vertical corners
        for (cx, cy, label) in [(0.0, 0.0, "front-left"), (0.0, room.width, "front-right"),
                                 (room.length, 0.0, "back-left"), (room.length, room.width, "back-right")] {
            items.append(RecommendationItem(
                category: .bassTrap, priority: cx == 0 ? .high : .medium,
                title: "Corner bass trap (\(label))",
                detail: "Floor-to-ceiling corner trap — corners are pressure maxima for every room mode.",
                location: Point3(x: cx, y: cy, z: room.height / 2)))
        }

        // 5) Reverb time / broadband absorption need (500 Hz reference)
        let surfaces = room.surfaces(tags: tags, at: .hz500)
        let (rt60, currentA) = ReverbTime.sabine(volume: room.volume, surfaces: surfaces)
        let addA = ReverbTime.absorptionToTarget(volume: room.volume, currentAbsorption: currentA, targetRT60: targetRT60)
        if addA > 0 {
            items.append(RecommendationItem(
                category: .absorption, priority: .medium,
                title: "Add broadband absorption",
                detail: String(format: "Measured-equivalent RT60 ≈ %.2f s vs a %.2f s target. Add roughly %.1f m² of broadband absorption (≈ %d panels of 60×120).",
                               rt60, targetRT60, addA, Int(ceil(addA / 0.72)))))
        }

        // 6) Worst modal problems (top axial modes + pile-ups)
        let pileups = RoomModes.pileups(modes)
        let problem = Array((pileups.flatMap { [$0.0, $0.1] } + modes.filter { $0.type == .axial })
            .reduce(into: [RoomMode]()) { acc, m in if !acc.contains(m) { acc.append(m) } }
            .prefix(6))
        if let worst = problem.first {
            items.append(RecommendationItem(
                category: .mode, priority: .medium,
                title: "Watch these bass resonances",
                detail: "Strongest room modes: " + problem.map { String(format: "%.0f Hz", $0.frequency) }.joined(separator: ", ")
                    + String(format: ". The %.0f Hz mode is the most likely to sound boomy.", worst.frequency)))
        }

        return RecommendationReport(
            setup: setup,
            problemModes: problem,
            rt60: rt60,
            absorptionToAddM2: max(addA, 0),
            items: items.sorted { $0.priority > $1.priority },
            isMeasured: false)
    }
}
