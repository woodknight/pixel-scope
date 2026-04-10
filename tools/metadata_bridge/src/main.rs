use exif::{Reader, Tag};
use std::env;
use std::fs::File;
use std::io::{BufReader, BufWriter, Write};
use std::path::Path;

const MAGIC: &[u8; 8] = b"PSMETA1\0";
const PAYLOAD_VERSION: u32 = 1;

#[derive(Debug, Clone)]
struct MetadataEntry {
    label: String,
    value: String,
}

fn write_u32_le(writer: &mut dyn Write, value: u32) -> Result<(), String> {
    writer
        .write_all(&value.to_le_bytes())
        .map_err(|err| err.to_string())
}

fn write_string(writer: &mut dyn Write, value: &str) -> Result<(), String> {
    let bytes = value.as_bytes();
    let len = u32::try_from(bytes.len()).map_err(|_| "String length exceeds u32 range.".to_string())?;
    write_u32_le(writer, len)?;
    writer.write_all(bytes).map_err(|err| err.to_string())
}

fn exif_field_value(exif: &exif::Exif, tag: Tag) -> Option<String> {
    exif.fields()
        .find(|field| field.tag == tag)
        .map(|field| field.display_value().with_unit(exif).to_string())
        .filter(|value| !value.trim().is_empty())
}

fn extract_metadata(input_path: &Path) -> Vec<MetadataEntry> {
    let file = match File::open(input_path) {
        Ok(file) => file,
        Err(_) => return Vec::new(),
    };
    let mut reader = BufReader::new(file);
    let exif = match Reader::new().read_from_container(&mut reader) {
        Ok(exif) => exif,
        Err(_) => return Vec::new(),
    };

    let mut entries = Vec::new();
    let mut push_tag = |label: &str, tag: Tag| {
        if let Some(value) = exif_field_value(&exif, tag) {
            let duplicate = entries
                .iter()
                .any(|entry: &MetadataEntry| entry.label == label && entry.value == value);
            if !duplicate {
                entries.push(MetadataEntry {
                    label: label.to_string(),
                    value,
                });
            }
        }
    };

    push_tag("Exposure Time", Tag::ExposureTime);
    push_tag("ISO", Tag::PhotographicSensitivity);
    push_tag("ISO", Tag::ISOSpeed);
    push_tag("Aperture", Tag::FNumber);
    push_tag("Exposure Bias", Tag::ExposureBiasValue);
    push_tag("Focal Length", Tag::FocalLength);
    push_tag("Date", Tag::DateTimeOriginal);
    push_tag("Lens Model", Tag::LensModel);
    push_tag("Software", Tag::Software);
    push_tag("Artist", Tag::Artist);
    push_tag("Copyright", Tag::Copyright);

    entries
}

fn write_payload(output_path: &Path, entries: &[MetadataEntry]) -> Result<(), String> {
    let file = File::create(output_path).map_err(|err| err.to_string())?;
    let mut writer = BufWriter::new(file);
    writer.write_all(MAGIC).map_err(|err| err.to_string())?;
    write_u32_le(&mut writer, PAYLOAD_VERSION)?;
    write_u32_le(
        &mut writer,
        u32::try_from(entries.len()).map_err(|_| "Metadata entry count exceeds u32 range.".to_string())?,
    )?;
    for entry in entries {
        write_string(&mut writer, &entry.label)?;
        write_string(&mut writer, &entry.value)?;
    }
    writer.flush().map_err(|err| err.to_string())
}

fn run() -> Result<(), String> {
    let mut args = env::args_os();
    let _program = args.next();
    let input_path = args
        .next()
        .ok_or_else(|| "Usage: metadata_bridge <input-path> <output-path>".to_string())?;
    let output_path = args
        .next()
        .ok_or_else(|| "Usage: metadata_bridge <input-path> <output-path>".to_string())?;

    if args.next().is_some() {
        return Err("Usage: metadata_bridge <input-path> <output-path>".to_string());
    }

    write_payload(Path::new(&output_path), &extract_metadata(Path::new(&input_path)))
}

fn main() {
    if let Err(message) = run() {
        eprintln!("{message}");
        std::process::exit(1);
    }
}
