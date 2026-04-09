// IARA V1 - Caixa paramétrica (OpenSCAD)
// Componentes: Arduino Uno, módulo TDS Keyestudio v1.0, OLED 0.96" I2C
// Exportar com: part = "base" ou part = "lid"

$fn = 48;

// =====================
// Seleção de peça
// =====================
part = "assembled"; // "base", "lid", "assembled"
show_cable_helpers = true; // true = mostra guias visuais das saídas de cabo

// =====================
// Dimensões gerais (mm)
// =====================
wall = 2.4;
bottom = 2.8;
lid_thickness = 2.4;
clearance = 0.35;
corner_r = 8;

inner_x = 140;
inner_y = 88;
inner_z = 32; // altura útil interna da base

outer_x = inner_x + wall * 2;
outer_y = inner_y + wall * 2;
base_h = bottom + inner_z;
lid_h = 13;

// =====================
// Arduino Uno R3 (aprox.)
// =====================
uno_x = 68.6;
uno_y = 53.4;
uno_h = 14; // folga para componentes + headers
standoff_d = 6.5;
standoff_h = 6;
standoff_hole_d = 2.9; // para parafuso M3 autoatarraxante leve

// Furação aproximada (referência no canto da placa)
uno_holes = [
    [3.5, 2.5],
    [66.1, 7.6],
    [66.1, 35.5],
    [14.0, 50.8]
];

// posição da placa dentro da caixa
uno_pos = [10, 10, bottom];

// recortes laterais para USB-B e jack DC (lado esquerdo da caixa)
usb_cut = [13, 11.5, 9.5]; // x(profundidade), y(largura), z(altura)
usb_cut_pos = [0, uno_pos[1] + 36, 10.5];

jack_cut_d = 11;
jack_cut_pos = [0, uno_pos[1] + 16.5, 11.5];

// =====================
// Módulo TDS Keyestudio v1.0 (aprox.)
// =====================
tds_x = 43;
tds_y = 32;
tds_pos = [86, 12, bottom];
tds_holes = [
    [3.5, 3.5],
    [39.5, 28.5]
];

// =====================
// OLED 0.96" (módulo I2C 4 pinos)
// =====================
oled_board_x = 27.5;
oled_board_y = 27.5;
oled_hole_pitch_x = 23;
oled_hole_pitch_y = 23;
oled_screen_x = 23.5;
oled_screen_y = 13.5;

// Saída do display (janela) na tampa:
// - oled_margin_top maior = janela mais para baixo
// - oled_offset_x positivo = desloca para a direita
oled_margin_top = 24;
oled_offset_x = 0;
oled_center = [outer_x / 2 + oled_offset_x, outer_y - oled_margin_top];

// Ajustes de montagem da placa OLED na tampa
oled_hole_d = 2.4;
oled_post_d = 5.2;
oled_post_h = 4.0;
oled_post_hole_d = 1.9; // para parafuso M2/M2.2

// Canal para passagem dos fios do OLED (lado superior da tampa)
oled_cable_notch_w = 8;
oled_cable_notch_h = 4;

echo("OLED center XY:", oled_center);
echo("Ajuste saída display: oled_margin_top / oled_offset_x");

// =====================
// Saídas de cabo
// =====================
// Lado direito da base
cable_tds_d = 6.6;
cable_temp_d = 4.8;
cable_cut_depth = wall + 2.2; // profundidade para atravessar a parede com folga

// Nota: o eixo do cilindro está no +X após rotate([0,90,0]), então iniciamos em
// outer_x - cable_cut_depth para garantir corte completo na lateral direita.
cable_tds_pos = [outer_x - cable_cut_depth, 24, 11.5];
cable_temp_pos = [outer_x - cable_cut_depth, 39, 10.5];

// =====================
// Parafusos da tampa
// =====================
post_d = 8;
post_hole_d = 3.0;
post_core_h = inner_z - 2;

post_xy = [
    [10, 10],
    [outer_x - 10, 10],
    [outer_x - 10, outer_y - 10],
    [10, outer_y - 10]
];

module rounded_box(x, y, z, r) {
    hull() {
        for (ix = [r, x - r])
            for (iy = [r, y - r])
                translate([ix, iy, 0]) cylinder(h = z, r = r);
    }
}

module shell_base() {
    difference() {
        rounded_box(outer_x, outer_y, base_h, corner_r);
        translate([wall, wall, bottom])
            rounded_box(inner_x, inner_y, inner_z + 0.2, max(corner_r - wall, 1));
    }
}

module lid_shell() {
    difference() {
        rounded_box(outer_x, outer_y, lid_h, corner_r);

        // cavidade da tampa
        translate([wall, wall, lid_thickness])
            rounded_box(inner_x, inner_y, lid_h, max(corner_r - wall, 1));

        // aba de encaixe
        translate([wall + clearance, wall + clearance, 0])
            rounded_box(inner_x - 2 * clearance, inner_y - 2 * clearance, 8, max(corner_r - wall - clearance, 1));

        // remover centro da aba
        translate([wall + 2.2, wall + 2.2, 0])
            rounded_box(inner_x - 4.4, inner_y - 4.4, 8.1, max(corner_r - wall - 2.2, 1));
    }
}

module board_standoffs(origin, holes) {
    for (h = holes) {
        translate([origin[0] + h[0], origin[1] + h[1], origin[2]])
            difference() {
                cylinder(h = standoff_h, d = standoff_d);
                translate([0, 0, -0.1]) cylinder(h = standoff_h + 0.2, d = standoff_hole_d);
            }
    }
}

module lid_posts_for_screws() {
    for (p = post_xy) {
        translate([p[0], p[1], bottom])
            difference() {
                cylinder(h = post_core_h, d = post_d);
                translate([0, 0, -0.1]) cylinder(h = post_core_h + 0.2, d = post_hole_d);
            }
    }
}

module cable_holes() {
    // TDS
    translate(cable_tds_pos)
        rotate([0, 90, 0]) cylinder(h = cable_cut_depth, d = cable_tds_d);

    // Temperatura
    translate(cable_temp_pos)
        rotate([0, 90, 0]) cylinder(h = cable_cut_depth, d = cable_temp_d);
}

module cable_holes_helper() {
    if (show_cable_helpers) {
        // Preview only (%): visível no editor, não exporta para STL
        %color([0.2, 0.9, 0.3, 0.45])
            translate(cable_tds_pos)
                rotate([0, 90, 0]) cylinder(h = cable_cut_depth, d = cable_tds_d);
        %color([0.2, 0.9, 0.3, 0.45])
            translate(cable_temp_pos)
                rotate([0, 90, 0]) cylinder(h = cable_cut_depth, d = cable_temp_d);
    }
}

module uno_side_ports() {
    // USB-B
    translate([usb_cut_pos[0] - 0.5, usb_cut_pos[1], usb_cut_pos[2]])
        cube([usb_cut[0] + 1, usb_cut[1], usb_cut[2]]);

    // Jack DC
    translate([jack_cut_pos[0] - 0.5, jack_cut_pos[1], jack_cut_pos[2]])
        rotate([0, 90, 0]) cylinder(h = wall + 1.5, d = jack_cut_d);
}

module lid_oled_window_and_holes() {
    // Janela do display
    translate([oled_center[0] - oled_screen_x / 2, oled_center[1] - oled_screen_y / 2, -0.2])
        cube([oled_screen_x, oled_screen_y, lid_thickness + 0.6]);

    // Furos da placa OLED
    for (sx = [-oled_hole_pitch_x / 2, oled_hole_pitch_x / 2])
        for (sy = [-oled_hole_pitch_y / 2, oled_hole_pitch_y / 2])
            translate([oled_center[0] + sx, oled_center[1] + sy, -0.2])
                cylinder(h = lid_thickness + 1.2, d = oled_hole_d);

    // Canal de saída do cabo do OLED (da região dos pinos para dentro da caixa)
    translate([oled_center[0] - oled_cable_notch_w / 2, oled_center[1] + oled_hole_pitch_y / 2 - 1, -0.2])
        cube([oled_cable_notch_w, oled_cable_notch_h, lid_thickness + 0.8]);
}

module lid_oled_posts() {
    // Postes de apoio/fixação da placa OLED na face interna da tampa
    for (sx = [-oled_hole_pitch_x / 2, oled_hole_pitch_x / 2])
        for (sy = [-oled_hole_pitch_y / 2, oled_hole_pitch_y / 2])
            translate([oled_center[0] + sx, oled_center[1] + sy, lid_thickness])
                difference() {
                    cylinder(h = oled_post_h, d = oled_post_d);
                    translate([0, 0, -0.1]) cylinder(h = oled_post_h + 0.2, d = oled_post_hole_d);
                }
}

module lid_screw_holes() {
    for (p = post_xy) {
        translate([p[0], p[1], -0.2])
            cylinder(h = lid_h + 0.4, d = 3.4);

        // rebaixo para cabeça do parafuso
        translate([p[0], p[1], lid_h - 2.2])
            cylinder(h = 2.5, d = 6.2);
    }
}

module base_part() {
    difference() {
        shell_base();
        cable_holes();
        uno_side_ports();
    }

    // Standoffs Arduino
    board_standoffs(uno_pos, uno_holes);

    // Standoffs TDS
    board_standoffs(tds_pos, tds_holes);

    // Postes para parafusos da tampa
    lid_posts_for_screws();
}

module lid_part() {
    union() {
        difference() {
            lid_shell();
            lid_oled_window_and_holes();
            lid_screw_holes();
        }
        lid_oled_posts();
    }
}

if (part == "base") {
    base_part();
    cable_holes_helper();
}

if (part == "lid") {
    lid_part();
}

if (part == "assembled") {
    color("#1f2937") base_part();
    cable_holes_helper();
    translate([0, 0, base_h + 0.2]) color("#334155") lid_part();
}
