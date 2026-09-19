-- Deterministic synthetic data for heavy-route startup and lifecycle tests.
--
-- This fixture is intentionally generated with SQLite recursive CTEs so the
-- row counts remain reviewable without checking in a large binary database.
PRAGMA foreign_keys = ON;

BEGIN TRANSACTION;

DELETE FROM speaking_eval_data;
DELETE FROM speaking_evaluations;
DELETE FROM roster_data;
DELETE FROM roster_columns;
DELETE FROM schedule_testing_blocks;
DELETE FROM testing_classes;
DELETE FROM class_intensive_times;
DELETE FROM class_times;
DELETE FROM intensive_slot_states;
DELETE FROM class_info;
DELETE FROM classes;
DELETE FROM teachers;
DELETE FROM native_english_teachers;
DELETE FROM gs_team;
DELETE FROM campuses;
DELETE FROM calendar_events;
DELETE FROM app_settings;
DELETE FROM sqlite_sequence;

WITH RECURSIVE ids(id) AS (
    SELECT 1
    UNION ALL
    SELECT id + 1 FROM ids WHERE id < 24
)
INSERT INTO teachers (
    id, teacher_kr, teacher_en, preferred_romanization, preferred_name,
    room_number, birthday, phone_number, wifi_name, wifi_password,
    internet_type, zoom_id, zoom_password, projection_type, notes
)
SELECT
    id,
    printf('Teacher %02d KR', id),
    printf('Teacher %02d', id),
    printf('Teacher-%02d', id),
    printf('Teacher %02d', id),
    printf('%03d', 300 + id),
    printf('19%02d-%02d-%02d', 80 + (id % 20), 1 + (id % 12), 1 + (id % 27)),
    printf('010-2000-%04d', id),
    printf('HeavyFixture-WiFi-%02d', id),
    'fixture-only',
    CASE id % 4
        WHEN 0 THEN 'WiFi'
        WHEN 1 THEN 'LAN'
        WHEN 2 THEN 'Both'
        ELSE 'N/A'
    END,
    printf('heavy-fixture-%02d', id),
    'fixture-only',
    CASE id % 4
        WHEN 0 THEN 'HDMI'
        WHEN 1 THEN 'Zoom'
        WHEN 2 THEN 'Any'
        ELSE 'N/A'
    END,
    'Synthetic large startup fixture teacher.'
FROM ids;

WITH RECURSIVE ids(id) AS (
    SELECT 1
    UNION ALL
    SELECT id + 1 FROM ids WHERE id < 96
)
INSERT INTO classes (id, name)
SELECT id, printf('Heavy Fixture Class %03d', id)
FROM ids;

WITH RECURSIVE ids(id) AS (
    SELECT 1
    UNION ALL
    SELECT id + 1 FROM ids WHERE id < 96
)
INSERT INTO class_info (
    class_id, teacher_id, class_grade, class_level, reading_book, essay_book,
    class_color, font_color, notes, time_filler_activities
)
SELECT
    id,
    ((id - 1) % 24) + 1,
    CASE id % 4
        WHEN 0 THEN 'M2'
        WHEN 1 THEN 'E4'
        WHEN 2 THEN 'E5'
        ELSE 'M1'
    END,
    printf('Level %02d', 1 + (id % 8)),
    printf('Heavy Reading %03d', id),
    printf('Heavy Essay %03d', id),
    CASE id % 6
        WHEN 0 THEN '#DDEBFF'
        WHEN 1 THEN '#E3F5E5'
        WHEN 2 THEN '#FFF0D6'
        WHEN 3 THEN '#EFE1FF'
        WHEN 4 THEN '#FFE1E1'
        ELSE '#E8EDF3'
    END,
    '#172B4D',
    'Synthetic large startup fixture class.',
    'Practice and discussion'
FROM ids;

WITH RECURSIVE
    class_ids(id) AS (
        SELECT 1
        UNION ALL
        SELECT id + 1 FROM class_ids WHERE id < 96
    ),
    slots(slot) AS (
        SELECT 1
        UNION ALL
        SELECT slot + 1 FROM slots WHERE slot < 8
    )
INSERT INTO class_times (class_id, day, start_time, end_time)
SELECT
    class_ids.id,
    CASE ((class_ids.id + slots.slot - 2) % 5)
        WHEN 0 THEN 'Monday'
        WHEN 1 THEN 'Tuesday'
        WHEN 2 THEN 'Wednesday'
        WHEN 3 THEN 'Thursday'
        ELSE 'Friday'
    END,
    printf('%d:00 PM', 3 + ((class_ids.id + slots.slot) % 4)),
    printf('%d:50 PM', 3 + ((class_ids.id + slots.slot) % 4))
FROM class_ids
CROSS JOIN slots;

WITH RECURSIVE ids(id) AS (
    SELECT 1
    UNION ALL
    SELECT id + 1 FROM ids WHERE id < 24
)
INSERT INTO class_intensive_times (class_id, day, start_time, end_time)
SELECT
    id,
    CASE ((id - 1) % 5)
        WHEN 0 THEN 'Monday'
        WHEN 1 THEN 'Tuesday'
        WHEN 2 THEN 'Wednesday'
        WHEN 3 THEN 'Thursday'
        ELSE 'Friday'
    END,
    printf('%d:00 AM', 8 + ((id - 1) / 5)),
    printf('%d:50 AM', 8 + ((id - 1) / 5))
FROM ids;

WITH RECURSIVE ids(id) AS (
    SELECT 1
    UNION ALL
    SELECT id + 1 FROM ids WHERE id < 5
)
INSERT INTO intensive_slot_states (day, start_time, state)
SELECT
    CASE id
        WHEN 1 THEN 'Monday'
        WHEN 2 THEN 'Tuesday'
        WHEN 3 THEN 'Wednesday'
        WHEN 4 THEN 'Thursday'
        ELSE 'Friday'
    END,
    '12:00 PM',
    'lunch'
FROM ids;

WITH RECURSIVE
    class_ids(id) AS (
        SELECT 1
        UNION ALL
        SELECT id + 1 FROM class_ids WHERE id < 96
    ),
    positions(position) AS (
        SELECT 0
        UNION ALL
        SELECT position + 1 FROM positions WHERE position < 2
    )
INSERT INTO roster_columns (class_id, name, position, width)
SELECT
    class_ids.id,
    CASE positions.position
        WHEN 0 THEN 'Student'
        WHEN 1 THEN 'School'
        ELSE 'Notes'
    END,
    positions.position,
    CASE positions.position
        WHEN 0 THEN 180
        WHEN 1 THEN 160
        ELSE 260
    END
FROM class_ids
CROSS JOIN positions;

WITH RECURSIVE
    class_ids(id) AS (
        SELECT 1
        UNION ALL
        SELECT id + 1 FROM class_ids WHERE id < 96
    ),
    rows(row_index) AS (
        SELECT 0
        UNION ALL
        SELECT row_index + 1 FROM rows WHERE row_index < 24
    ),
    positions(col_index) AS (
        SELECT 0
        UNION ALL
        SELECT col_index + 1 FROM positions WHERE col_index < 2
    )
INSERT INTO roster_data (class_id, row_index, col_index, value)
SELECT
    class_ids.id,
    rows.row_index,
    positions.col_index,
    CASE positions.col_index
        WHEN 0 THEN printf('Student %03d-%02d', class_ids.id, rows.row_index + 1)
        WHEN 1 THEN printf('School %02d', 1 + (class_ids.id % 12))
        ELSE printf('Heavy roster note %03d-%02d', class_ids.id, rows.row_index + 1)
    END
FROM class_ids
CROSS JOIN rows
CROSS JOIN positions;

WITH RECURSIVE ids(id) AS (
    SELECT 1
    UNION ALL
    SELECT id + 1 FROM ids WHERE id < 20
)
INSERT INTO speaking_evaluations (id, class_id, evaluation_name)
SELECT id, id, printf('Heavy Term %02d', id)
FROM ids;

WITH RECURSIVE
    evaluation_ids(evaluation_id) AS (
        SELECT 1
        UNION ALL
        SELECT evaluation_id + 1 FROM evaluation_ids WHERE evaluation_id < 20
    ),
    rows(row_index) AS (
        SELECT 0
        UNION ALL
        SELECT row_index + 1 FROM rows WHERE row_index < 29
    )
INSERT INTO speaking_eval_data (
    evaluation_id, row_index, col_0, col_1, col_2, col_3, col_4, col_5,
    col_6, col_7, col_8, col_9, col_10
)
SELECT
    evaluation_ids.evaluation_id,
    rows.row_index,
    printf('Student %03d', rows.row_index + 1),
    printf('Heavy Eval %02d', evaluation_ids.evaluation_id),
    printf('Score %02d', (rows.row_index + evaluation_ids.evaluation_id) % 101),
    'Speaking',
    'Listening',
    'Vocabulary',
    'Grammar',
    'Fluency',
    'Notes',
    'Teacher',
    'Fixture'
FROM evaluation_ids
CROSS JOIN rows;

INSERT INTO campuses (id, name, building_name, address, phone_number)
VALUES
    (1, 'Heavy Fixture Main', 'Main Building', '1 Fixture Road', '02-4000-0001'),
    (2, 'Heavy Fixture Annex', 'Annex Building', '2 Fixture Road', '02-4000-0002'),
    (3, 'Heavy Fixture West', 'West Building', '3 Fixture Road', '02-4000-0003');

WITH RECURSIVE ids(id) AS (
    SELECT 1
    UNION ALL
    SELECT id + 1 FROM ids WHERE id < 180
)
INSERT INTO calendar_events (
    id, title, event_type, time_status, repeat_series_id, all_day,
    start_date, start_time, end_date, end_time
)
SELECT
    id,
    printf('Heavy Fixture Event %03d', id),
    CASE id % 5
        WHEN 0 THEN 'Vacation'
        WHEN 1 THEN 'Holiday'
        WHEN 2 THEN 'Workshop'
        WHEN 3 THEN 'Meeting'
        ELSE 'Other'
    END,
    CASE id % 3
        WHEN 0 THEN 'Timed'
        WHEN 1 THEN 'Unknown'
        ELSE 'Unconfirmed'
    END,
    printf('heavy-series-%02d', (id - 1) / 7),
    id % 2,
    date('2026-01-01', printf('+%d day', id - 1)),
    printf('%02d:00', 8 + (id % 8)),
    date('2026-01-01', printf('+%d day', id - 1)),
    printf('%02d:50', 8 + (id % 8))
FROM ids;

INSERT INTO app_settings (key, value) VALUES
    ('custom_colors', '["#DDEBFF","#E3F5E5","#FFF0D6","#EFE1FF"]'),
    ('myInfo/campus', 'Heavy Fixture Main'),
    ('myInfo/name', 'Teacher 01'),
    ('myInfo/zoomLoginId', 'fixture-only'),
    ('myInfo/zoomNotAvailable', '0'),
    ('myInfo/zoomPassword', 'fixture-only'),
    ('schedule_display_mode', 'regular'),
    ('schedule_show_all_hours_v2', 'true'),
    ('schedule_show_korean_teacher_english_names', 'true'),
    ('schedule_show_weekends', 'true'),
    ('schedule_testing_affects_m1', 'false'),
    ('schedule_use_24h', 'false');

COMMIT;

VACUUM;
