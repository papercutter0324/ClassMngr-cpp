-- Legacy profile source for compatibility and migration tests.
--
-- This is intentionally an early, partially populated schema. The current
-- DatabaseSchemaManager must create missing tables, add current columns,
-- repair the unassigned teacher, and apply the latest constraints.
PRAGMA foreign_keys = OFF;

BEGIN TRANSACTION;

CREATE TABLE classes (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT
);

CREATE TABLE teachers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    teacher_kr TEXT,
    teacher_en TEXT,
    room_number TEXT,
    wifi_name TEXT,
    wifi_password TEXT,
    zoom_id TEXT,
    zoom_password TEXT,
    notes TEXT
);

CREATE TABLE class_info (
    class_id INTEGER PRIMARY KEY,
    teacher_id INTEGER,
    class_grade TEXT,
    class_level TEXT,
    reading_book TEXT,
    essay_book TEXT,
    class_color TEXT DEFAULT '#FFFFFF',
    font_color TEXT DEFAULT '#000000',
    notes TEXT
);

CREATE TABLE class_times (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    class_id INTEGER,
    day TEXT,
    start_time TEXT,
    end_time TEXT
);

INSERT INTO teachers (
    id, teacher_kr, teacher_en, room_number, wifi_name, wifi_password,
    zoom_id, zoom_password, notes
) VALUES
    (1, 'Legacy Teacher', 'Legacy Teacher', '201', 'Legacy-WiFi',
     'fixture-only', 'legacy.fixture', 'fixture-only',
     'Synthetic legacy compatibility fixture.');

INSERT INTO classes (id, name)
VALUES (1, 'Legacy E4');

-- A non-positive teacher id is repaired to NULL by the legacy preflight.
INSERT INTO class_info (
    class_id, teacher_id, class_grade, class_level, reading_book, essay_book,
    class_color, font_color, notes
) VALUES (
    1, -1, 'E4', 'Blue', 'Legacy Reading', 'Legacy Essay',
    '#DDEBFF', '#172B4D', 'Synthetic legacy class.'
);

INSERT INTO class_times (class_id, day, start_time, end_time)
VALUES (1, 'Monday', '4:00 PM', '4:50 PM');

-- The omitted PRAGMA user_version remains at zero, so migration 1 creates the
-- tables omitted above before later migrations add columns and constraints.

COMMIT;
