-- Deterministic data for Testing-copy.tps, the startup-regression profile.
--
-- Keep this fixture synthetic.  It deliberately exercises the initial
-- My Workspace / Schedule route with more than one teacher, class, schedule
-- type, roster, and saved schedule preference, without storing user data.
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

INSERT INTO teachers (
    id, teacher_kr, teacher_en, preferred_romanization, preferred_name,
    room_number, birthday, phone_number, wifi_name, wifi_password,
    internet_type, zoom_id, zoom_password, projection_type, notes
) VALUES
    (1, '김다은', 'Dana Kim', 'Kim Da-eun', 'Dana', '301', '1990-01-14',
     '010-1000-0001', 'Fixture-WiFi', 'fixture-only', 'WiFi', 'dana.fixture',
     'fixture-only', 'HDMI', 'Synthetic startup fixture teacher.'),
    (2, '이준', 'Jordan Lee', 'Lee Jun', 'Jordan', '302', '1991-03-09',
     '010-1000-0002', 'Fixture-WiFi', 'fixture-only', 'LAN', 'jordan.fixture',
     'fixture-only', 'Zoom', 'Synthetic startup fixture teacher.'),
    (3, '박민서', 'Minseo Park', 'Park Min-seo', 'Minseo', '303', '1992-07-21',
     '010-1000-0003', 'Fixture-WiFi', 'fixture-only', 'Both', 'minseo.fixture',
     'fixture-only', 'Any', 'Synthetic startup fixture teacher.'),
    (4, '최유진', 'Eugene Choi', 'Choi Yu-jin', 'Eugene', '304', '1993-11-02',
     '010-1000-0004', 'Fixture-WiFi', 'fixture-only', 'WiFi', 'eugene.fixture',
     'fixture-only', 'HDMI', 'Synthetic startup fixture teacher.');

INSERT INTO classes (id, name) VALUES
    (1, 'Bluebird E4'),
    (2, 'Falcon E5'),
    (3, 'Hawk M1'),
    (4, 'Owl M2'),
    (5, 'Robin E6'),
    (6, 'Swift M3'),
    (7, 'Cedar Intensive'),
    (8, 'Maple Intensive');

INSERT INTO class_info (
    class_id, teacher_id, class_grade, class_level, reading_book, essay_book,
    class_color, font_color, notes, time_filler_activities
) VALUES
    (1, 1, 'E4', 'Blue', 'Reading A', 'Essay A', '#DDEBFF', '#172B4D',
     'Synthetic regular class.', 'Word ladder'),
    (2, 2, 'E5', 'Green', 'Reading B', 'Essay B', '#E3F5E5', '#173D22',
     'Synthetic regular class.', 'Vocabulary race'),
    (3, 3, 'M1', 'Orange', 'Reading C', 'Essay C', '#FFF0D6', '#5B3500',
     'Synthetic regular class.', 'Discussion prompts'),
    (4, 4, 'M2', 'Purple', 'Reading D', 'Essay D', '#EFE1FF', '#32124D',
     'Synthetic regular class.', 'Peer review'),
    (5, 1, 'E6', 'Red', 'Reading E', 'Essay E', '#FFE1E1', '#5B1010',
     'Synthetic regular class.', 'Reading relay'),
    (6, 2, 'M3', 'Silver', 'Reading F', 'Essay F', '#E8EDF3', '#1F2D3D',
     'Synthetic regular class.', 'Writing sprint'),
    (7, 3, 'E5', 'Intensive A', 'Intensive Reading A', 'Intensive Essay A',
     '#DFF7F3', '#0C3B35', 'Synthetic intensive class.', 'Mini debate'),
    (8, 4, 'M1', 'Intensive B', 'Intensive Reading B', 'Intensive Essay B',
     '#FFF6CC', '#4A3B00', 'Synthetic intensive class.', 'Practice test');

INSERT INTO class_times (class_id, day, start_time, end_time) VALUES
    (1, 'Monday',    '4:00 PM', '4:50 PM'),
    (2, 'Monday',    '5:00 PM', '5:50 PM'),
    (3, 'Tuesday',   '4:00 PM', '4:50 PM'),
    (4, 'Tuesday',   '5:00 PM', '5:50 PM'),
    (5, 'Wednesday', '4:00 PM', '4:50 PM'),
    (6, 'Wednesday', '5:00 PM', '5:50 PM'),
    (1, 'Thursday',  '4:00 PM', '4:50 PM'),
    (2, 'Thursday',  '5:00 PM', '5:50 PM'),
    (3, 'Friday',    '4:00 PM', '4:50 PM'),
    (4, 'Friday',    '5:00 PM', '5:50 PM');

INSERT INTO class_intensive_times (class_id, day, start_time, end_time) VALUES
    (7, 'Monday',    '9:00 AM',  '9:50 AM'),
    (8, 'Tuesday',   '10:00 AM', '10:50 AM'),
    (7, 'Wednesday', '11:00 AM', '11:50 AM'),
    (8, 'Thursday',  '1:00 PM',  '1:50 PM');

INSERT INTO intensive_slot_states (day, start_time, state) VALUES
    ('Monday', '12:00 PM', 'lunch'),
    ('Tuesday', '1:00 PM', 'essay'),
    ('Friday', '3:00 PM', 'testing');

INSERT INTO roster_columns (class_id, name, position, width) VALUES
    (1, 'Student', 0, 170),
    (1, 'School',  1, 150),
    (1, 'Level',   2, 90),
    (1, 'Notes',   3, 220),
    (7, 'Student', 0, 170),
    (7, 'School',  1, 150),
    (7, 'Level',   2, 90),
    (7, 'Notes',   3, 220);

INSERT INTO roster_data (class_id, row_index, col_index, value) VALUES
    (1, 0, 0, 'Avery'), (1, 0, 1, 'River School'), (1, 0, 2, 'E4'),
    (1, 1, 0, 'Blair'), (1, 1, 1, 'River School'), (1, 1, 2, 'E4'),
    (1, 2, 0, 'Casey'), (1, 2, 1, 'River School'), (1, 2, 2, 'E4'),
    (1, 3, 0, 'Devon'), (1, 3, 1, 'River School'), (1, 3, 2, 'E4'),
    (7, 0, 0, 'Emery'), (7, 0, 1, 'Hill School'), (7, 0, 2, 'E5'),
    (7, 1, 0, 'Finley'), (7, 1, 1, 'Hill School'), (7, 1, 2, 'E5'),
    (7, 2, 0, 'Gray'), (7, 2, 1, 'Hill School'), (7, 2, 2, 'E5'),
    (7, 3, 0, 'Harper'), (7, 3, 1, 'Hill School'), (7, 3, 2, 'E5');

INSERT INTO app_settings (key, value) VALUES
    ('custom_colors', '["#DDEBFF","#E3F5E5","#FFF0D6","#EFE1FF"]'),
    ('myInfo/campus', 'Fixture Campus'),
    ('myInfo/name', 'Jordan Lee'),
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
