#pragma once
namespace inde::persistence::schema_sql {
constexpr auto schema_version_17 = R"sql(
ALTER TABLE documents ADD COLUMN inline_commands TEXT NOT NULL DEFAULT '[]';
ALTER TABLE document_format_spans RENAME TO document_format_spans_v16;
DROP INDEX document_format_spans_project_idx;
CREATE TABLE document_format_spans (
 project_id TEXT NOT NULL, document_id TEXT NOT NULL,
 style TEXT NOT NULL CHECK(style IN ('bold','italic','underline','strikethrough','heading','subheading','quote',
  'heading2','heading3','heading4','heading5','heading6','align-left','align-center','align-right','align-justify',
  'font-family','font-size','foreground')),
 start_offset INTEGER NOT NULL CHECK(start_offset>=0), end_offset INTEGER NOT NULL CHECK(end_offset>start_offset),
 value TEXT NOT NULL DEFAULT '',
 PRIMARY KEY(document_id,style,start_offset,end_offset,value),
 FOREIGN KEY(document_id,project_id) REFERENCES documents(id,project_id) ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED
);
INSERT INTO document_format_spans(project_id,document_id,style,start_offset,end_offset)
 SELECT project_id,document_id,style,start_offset,end_offset FROM document_format_spans_v16;
DROP TABLE document_format_spans_v16;
CREATE INDEX document_format_spans_project_idx ON document_format_spans(project_id,document_id,start_offset);
)sql";
}
