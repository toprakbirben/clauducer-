{
    "patcher": {
        "fileversion": 1,
        "appversion": {
            "major": 9,
            "minor": 1,
            "revision": 4,
            "architecture": "x64",
            "modernui": 1
        },
        "classnamespace": "box",
        "rect": [ 272.0, 95.0, 850.0, 1060.0 ],
        "openinpresentation": 1,
        "boxes": [
            {
                "box": {
                    "id": "obj-23",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 533.0, 715.0, 61.0, 22.0 ],
                    "text": "print error"
                }
            },
            {
                "box": {
                    "id": "obj-22",
                    "maxclass": "textedit",
                    "numinlets": 1,
                    "numoutlets": 4,
                    "outlettype": [ "", "int", "", "" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 372.5, 715.0, 100.0, 50.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 330.0, 108.0, 190.0, 55.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-20",
                    "maxclass": "textedit",
                    "numinlets": 1,
                    "numoutlets": 4,
                    "outlettype": [ "", "int", "", "" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 270.0, 715.0, 100.0, 50.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 170.0, 108.0, 150.0, 55.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-19",
                    "maxclass": "textedit",
                    "numinlets": 1,
                    "numoutlets": 4,
                    "outlettype": [ "", "int", "", "" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 169.0, 715.0, 100.0, 50.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 10.0, 108.0, 150.0, 55.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-16",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 8,
                    "outlettype": [ "", "", "", "", "", "", "", "" ],
                    "patching_rect": [ 290.5, 671.0, 400.0, 22.0 ],
                    "text": "route analyzed wanted results_text selected_uuid selected_name downloaded error"
                }
            },
            {
                "box": {
                    "id": "obj-15",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "" ],
                    "patching_rect": [ 290.5, 598.0, 660.0, 22.0 ],
                    "saved_object_attributes": {
                        "autostart": 1,
                        "defer": 0,
                        "node_bin_path": "",
                        "npm_bin_path": "",
                        "watch": 1
                    },
                    "text": "node.script match-client.js @autostart 1 @watch 1",
                    "textfile": {
                        "filename": "match-client.js",
                        "flags": 0,
                        "embed": 0,
                        "autowatch": 1
                    }
                }
            },
            {
                "box": {
                    "id": "obj-14",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 290.5, 538.0, 89.0, 22.0 ],
                    "text": "prepend match"
                }
            },
            {
                "box": {
                    "id": "obj-13",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 290.5, 491.0, 53.0, 22.0 ],
                    "text": "pack s s"
                }
            },
            {
                "box": {
                    "id": "obj-12",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 346.0, 420.0, 104.0, 22.0 ],
                    "text": "value prompt_text"
                }
            },
            {
                "box": {
                    "id": "obj-11",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 221.0, 420.0, 100.0, 22.0 ],
                    "text": "value audio_path"
                }
            },
            {
                "box": {
                    "id": "obj-10",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "bang", "bang" ],
                    "patching_rect": [ 221.0, 359.0, 144.0, 22.0 ],
                    "text": "t b b"
                }
            },
            {
                "box": {
                    "id": "obj-9",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 221.0, 268.0, 67.0, 67.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 430.0, 10.0, 30.0, 30.0 ]
                }
            },
            {
                "box": {
                    "color": [ 0.047058823529411764, 0.050980392156862744, 0.06274509803921569, 1.0 ],
                    "id": "obj-8",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 265.0, 206.0, 104.0, 22.0 ],
                    "saved_attribute_attributes": {
                        "color": {
                            "expression": "themecolor.live_contrast_frame"
                        }
                    },
                    "saved_newobj_attribute_attributes": {
                        "color": {
                            "expression": "themecolor.live_contrast_frame"
                        }
                    },
                    "text": "value prompt_text"
                }
            },
            {
                "box": {
                    "bangmode": 1,
                    "id": "obj-7",
                    "keymode": 1,
                    "linecount": 3,
                    "maxclass": "textedit",
                    "numinlets": 1,
                    "numoutlets": 4,
                    "outlettype": [ "", "int", "", "" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 265.0, 89.0, 100.0, 50.0 ],
                    "text": "hoover dark pad under the drum loop.",
                    "presentation": 1,
                    "presentation_rect": [ 110.0, 10.0, 300.0, 45.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-6",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 125.0, 206.0, 100.0, 22.0 ],
                    "text": "value audio_path"
                }
            },
            {
                "box": {
                    "id": "obj-5",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "patching_rect": [ 125.0, 145.0, 67.0, 22.0 ],
                    "text": "opendialog"
                }
            },
            {
                "box": {
                    "id": "obj-4",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 125.0, 26.0, 74.0, 74.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 10.0, 10.0, 30.0, 30.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-24",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 169.0, 671.0, 66.0, 22.0 ],
                    "text": "prepend set"
                }
            },
            {
                "box": {
                    "id": "obj-25",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 270.0, 671.0, 66.0, 22.0 ],
                    "text": "prepend set"
                }
            },
            {
                "box": {
                    "id": "obj-26",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 372.5, 671.0, 66.0, 22.0 ],
                    "text": "prepend set"
                }
            },
            {
                "box": {
                    "id": "obj-27",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "text": "Received audio",
                    "patching_rect": [ 169.0, 690.0, 120.0, 18.0 ],
                    "fontsize": 11.0,
                    "presentation": 1,
                    "presentation_rect": [ 10.0, 90.0, 150.0, 16.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-28",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "text": "Wanted sound",
                    "patching_rect": [ 270.0, 690.0, 120.0, 18.0 ],
                    "fontsize": 11.0,
                    "presentation": 1,
                    "presentation_rect": [ 170.0, 90.0, 150.0, 16.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-29",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "text": "Results (numbered)",
                    "patching_rect": [ 372.5, 690.0, 140.0, 18.0 ],
                    "fontsize": 11.0,
                    "presentation": 1,
                    "presentation_rect": [ 330.0, 90.0, 190.0, 16.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-30",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "text": "Errors",
                    "patching_rect": [ 533.0, 690.0, 80.0, 18.0 ],
                    "fontsize": 11.0,
                    "presentation": 1,
                    "presentation_rect": [ 530.0, 90.0, 220.0, 16.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-31",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "text": "Result #",
                    "patching_rect": [ 169.0, 790.0, 80.0, 18.0 ],
                    "fontsize": 11.0,
                    "presentation": 1,
                    "presentation_rect": [ 10.0, 175.0, 80.0, 16.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-32",
                    "maxclass": "number",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "int", "bang" ],
                    "patching_rect": [ 169.0, 812.0, 50.0, 22.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 10.0, 193.0, 50.0, 22.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-34",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "text": "Selected",
                    "patching_rect": [ 270.0, 790.0, 80.0, 18.0 ],
                    "fontsize": 11.0,
                    "presentation": 1,
                    "presentation_rect": [ 80.0, 175.0, 150.0, 16.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-35",
                    "maxclass": "textedit",
                    "numinlets": 1,
                    "numoutlets": 4,
                    "outlettype": [ "", "int", "", "" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 270.0, 812.0, 150.0, 45.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 80.0, 193.0, 150.0, 42.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-37",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "text": "Download (spends a credit)",
                    "patching_rect": [ 450.0, 790.0, 130.0, 18.0 ],
                    "fontsize": 11.0,
                    "presentation": 1,
                    "presentation_rect": [ 250.0, 175.0, 150.0, 16.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-36",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 450.0, 812.0, 24.0, 24.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 250.0, 193.0, 30.0, 30.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-38",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "text": "Downloaded to",
                    "patching_rect": [ 580.0, 790.0, 100.0, 18.0 ],
                    "fontsize": 11.0,
                    "presentation": 1,
                    "presentation_rect": [ 300.0, 175.0, 150.0, 16.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-39",
                    "maxclass": "textedit",
                    "numinlets": 1,
                    "numoutlets": 4,
                    "outlettype": [ "", "int", "", "" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 580.0, 812.0, 180.0, 45.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 300.0, 193.0, 220.0, 42.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-33",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 169.0, 880.0, 140.0, 22.0 ],
                    "text": "prepend select_result"
                }
            },
            {
                "box": {
                    "id": "obj-46",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 270.0, 880.0, 66.0, 22.0 ],
                    "text": "prepend set"
                }
            },
            {
                "box": {
                    "id": "obj-45",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 580.0, 880.0, 66.0, 22.0 ],
                    "text": "prepend set"
                }
            },
            {
                "box": {
                    "id": "obj-40",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 330.0, 880.0, 130.0, 22.0 ],
                    "text": "value selected_uuid"
                }
            },
            {
                "box": {
                    "id": "obj-41",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 330.0, 915.0, 130.0, 22.0 ],
                    "text": "value selected_name"
                }
            },
            {
                "box": {
                    "id": "obj-42",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "bang", "bang" ],
                    "patching_rect": [ 470.0, 880.0, 80.0, 22.0 ],
                    "text": "t b b"
                }
            },
            {
                "box": {
                    "id": "obj-43",
                    "maxclass": "newobj",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 330.0, 950.0, 120.0, 22.0 ],
                    "text": "pack s s"
                }
            },
            {
                "box": {
                    "id": "obj-44",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 330.0, 985.0, 140.0, 22.0 ],
                    "text": "prepend download"
                }
            },
            {
                "box": {
                    "id": "obj-47",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "text": "Pick audio",
                    "patching_rect": [ 125.0, 1010.0, 90.0, 16.0 ],
                    "fontsize": 10.0,
                    "presentation": 1,
                    "presentation_rect": [ 10.0, 42.0, 90.0, 16.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-48",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "text": "Prompt",
                    "patching_rect": [ 265.0, 1010.0, 90.0, 16.0 ],
                    "fontsize": 10.0,
                    "presentation": 1,
                    "presentation_rect": [ 110.0, 57.0, 90.0, 16.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-49",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "text": "Match",
                    "patching_rect": [ 400.0, 1010.0, 60.0, 16.0 ],
                    "fontsize": 10.0,
                    "presentation": 1,
                    "presentation_rect": [ 430.0, 42.0, 60.0, 16.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-50",
                    "maxclass": "textedit",
                    "numinlets": 1,
                    "numoutlets": 4,
                    "outlettype": [ "", "int", "", "" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 533.0, 1010.0, 220.0, 55.0 ],
                    "presentation": 1,
                    "presentation_rect": [ 530.0, 108.0, 220.0, 55.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-51",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 533.0, 1045.0, 66.0, 22.0 ],
                    "text": "prepend set"
                }
            }
        ],
        "lines": [
            { "patchline": { "destination": [ "obj-11", 0 ], "source": [ "obj-10", 0 ] } },
            { "patchline": { "destination": [ "obj-12", 0 ], "source": [ "obj-10", 1 ] } },
            { "patchline": { "destination": [ "obj-13", 0 ], "source": [ "obj-11", 0 ] } },
            { "patchline": { "destination": [ "obj-13", 1 ], "source": [ "obj-12", 0 ] } },
            { "patchline": { "destination": [ "obj-14", 0 ], "source": [ "obj-13", 0 ] } },
            { "patchline": { "destination": [ "obj-15", 0 ], "source": [ "obj-14", 0 ] } },
            { "patchline": { "destination": [ "obj-16", 0 ], "source": [ "obj-15", 0 ] } },
            { "patchline": { "destination": [ "obj-24", 0 ], "source": [ "obj-16", 0 ] } },
            { "patchline": { "destination": [ "obj-19", 0 ], "source": [ "obj-24", 0 ] } },
            { "patchline": { "destination": [ "obj-25", 0 ], "source": [ "obj-16", 1 ] } },
            { "patchline": { "destination": [ "obj-20", 0 ], "source": [ "obj-25", 0 ] } },
            { "patchline": { "destination": [ "obj-26", 0 ], "source": [ "obj-16", 2 ] } },
            { "patchline": { "destination": [ "obj-22", 0 ], "source": [ "obj-26", 0 ] } },
            { "patchline": { "destination": [ "obj-40", 0 ], "source": [ "obj-16", 3 ] } },
            { "patchline": { "destination": [ "obj-41", 0 ], "source": [ "obj-16", 4 ] } },
            { "patchline": { "destination": [ "obj-46", 0 ], "source": [ "obj-16", 4 ] } },
            { "patchline": { "destination": [ "obj-35", 0 ], "source": [ "obj-46", 0 ] } },
            { "patchline": { "destination": [ "obj-45", 0 ], "source": [ "obj-16", 5 ] } },
            { "patchline": { "destination": [ "obj-39", 0 ], "source": [ "obj-45", 0 ] } },
            { "patchline": { "destination": [ "obj-23", 0 ], "source": [ "obj-16", 6 ] } },
            { "patchline": { "destination": [ "obj-51", 0 ], "source": [ "obj-16", 6 ] } },
            { "patchline": { "destination": [ "obj-50", 0 ], "source": [ "obj-51", 0 ] } },
            { "patchline": { "destination": [ "obj-5", 0 ], "source": [ "obj-4", 0 ] } },
            { "patchline": { "destination": [ "obj-6", 0 ], "source": [ "obj-5", 0 ] } },
            { "patchline": { "destination": [ "obj-8", 0 ], "midpoints": [ 274.5, 141.0, 274.5, 141.0 ], "source": [ "obj-7", 0 ] } },
            { "patchline": { "destination": [ "obj-10", 0 ], "source": [ "obj-9", 0 ] } },
            { "patchline": { "destination": [ "obj-33", 0 ], "source": [ "obj-32", 0 ] } },
            { "patchline": { "destination": [ "obj-15", 0 ], "source": [ "obj-33", 0 ] } },
            { "patchline": { "destination": [ "obj-42", 0 ], "source": [ "obj-36", 0 ] } },
            { "patchline": { "destination": [ "obj-40", 0 ], "source": [ "obj-42", 0 ] } },
            { "patchline": { "destination": [ "obj-41", 0 ], "source": [ "obj-42", 1 ] } },
            { "patchline": { "destination": [ "obj-43", 0 ], "source": [ "obj-40", 0 ] } },
            { "patchline": { "destination": [ "obj-43", 1 ], "source": [ "obj-41", 0 ] } },
            { "patchline": { "destination": [ "obj-44", 0 ], "source": [ "obj-43", 0 ] } },
            { "patchline": { "destination": [ "obj-15", 0 ], "source": [ "obj-44", 0 ] } }
        ],
        "autosave": 0,
        "oscreceiveudpport": 0
    }
}
