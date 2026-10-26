-- Synthetic starting settings for the Next Initial Setup personal-details
-- persistence acceptance. The test creates the real .tps schema first.
INSERT INTO app_settings (key, value) VALUES
    ('myInfo/name', 'Fixture Teacher'),
    ('myInfo/campus', 'Fixture Campus'),
    ('myInfo/zoomLoginId', 'fixture.zoom@example.test'),
    ('myInfo/zoomPassword', 'fixture-only'),
    ('myInfo/zoomNotAvailable', '0'),
    ('myInfo/signatureImage', ''),
    ('myInfo/signatureMode', '0'),
    ('myInfo/typedSignatureText', ''),
    ('myInfo/typedSignatureFont', '0'),
    ('fixture/unrelatedSetting', 'preserve-this-value')
ON CONFLICT(key) DO UPDATE SET value=excluded.value;
