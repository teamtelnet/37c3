use rand::Rng;
use std::fs::File;
use std::io::{self, BufRead};
use std::path::Path;
use std::error::Error;
use std::env;
use std::process;

// read chars from file ([a-z][A-Z][0-9][#!])
fn read_chars_from_file(path: &str) -> Result<Vec<char>, Box<dyn Error>> {
    let path = Path::new(path);
    let file = match File::open(&path) {
        Ok(file) => file,
        Err(error) => panic!("Problem opening the file: {:?}", error),
    };
    let reader = io::BufReader::new(file);
    let mut result: Vec<char> = Vec::new();
    for (_index, line) in reader.lines().enumerate() {
        let line = match line {
            Ok(line) => line,
            Err(error) => panic!("Parsing error {:?}", error),
        };
        for c in line.chars() {
            result.push(c);
        }
    }
    return Ok(result);
}
fn create_wg_password(characters: Vec<char>) -> String {
    let pwd_len = 10;
    let mut result: String = String::new();
    for _i in 0..pwd_len {
        result.push(characters[rand::thread_rng().gen_range(0..=pwd_len)])
    }
    return result;
}
fn create_token() -> String {
    let mut result = String::new();
    for _i in 0..10 {
        result.push((rand::thread_rng().gen_range(65..=90) as u8) as char);
    }
    return result;
}

// the main und btw: AFD VERBOT, JETZT!1!elf
fn main() {
    let args: Vec<String> = env::args().collect();

    if args.len() < 2 {
        eprintln!("Error: user missing");
        process::exit(1); 
    }

    loop {
        // read file with all possible chars (random order)
        let result = match read_chars_from_file("../share/passchars") {
            Ok(result) => result,
            Err(e) => {
                println!("{e}");
                break;
            },
        };

        // write wg-config
        let mut command = process::Command::new("sh");
        command.arg("../wireguard/add-client.sh");
        command.arg(args[1].clone()).arg(create_token()).arg(create_wg_password(result));
        match command.output() {
            Ok(_output) => (),
            Err(error) => panic!("Error executing script: {:?}", error),
        }
        break;
    }
}

