use manimatio_rust_lib::{render_typst_sequence, render_typst_to_svg};

fn main() {
    let typst_code = "= Hello from Rust\n\nThis is a test of the Typst rendering bridge!";

    let result = render_typst_to_svg(typst_code);

    if !result.error.is_empty() {
        eprintln!("Error rendering SVG: {}", result.error);
    } else {
        println!("Successfully rendered SVG ({} bytes).", result.svg.len());
        let preview_len = std::cmp::min(100, result.svg.len());
        println!("Preview:\n{}...\n", &result.svg[..preview_len]);
    }

    let template_code = "#let users = sys.inputs.users\n\
         #for user in users [\n\
         = Welcome, #user!\n\
         #pagebreak()\n\
         ]";

    let params_json = r#"{ "users": ["Alice", "Bob", "Charlie"] }"#;

    let seq_result = render_typst_sequence(template_code, params_json);

    if !seq_result.error.is_empty() {
        eprintln!("Error rendering sequence: {}", seq_result.error);
    } else {
        println!(
            "Successfully rendered sequence with {} pages.",
            seq_result.svgs.len()
        );
        for (i, svg) in seq_result.svgs.iter().enumerate() {
            println!("Page {} SVG size: {} bytes.", i + 1, svg.len());
        }
    }
}
