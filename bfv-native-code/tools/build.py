"""Compile editable native AI and append it to an original, hash-checked PE copy."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import pefile

PROJECT = Path(__file__).resolve().parents[1]
GAME = Path(os.environ.get('BFV_GAME_DIR', PROJECT.parent)).resolve()
COMPILERS = Path(os.environ.get('MINGW32_BIN', 'C:/msys64/mingw32/bin'))
TARGETS = {
    'client': dict(file='BfVietnam.exe', output='BfVietnam-editable.exe',
        sha='79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5',
        list_find_word=0x9bf7c0,
        artillery_category_score=0x9a007a,
        artillery_category_alternate=0x9a01f9,
        artillery_category_scale=0xb5d484,
        target_handle_eligible=0x97ede0,
        artillery_alternate_gate=0x9a01f9,
        artillery_alternate_accept=0x9a0264,
        artillery_nested_finish=0x9a03cc,
        artillery_next_target=0x963a20,
        artillery_lookup_target=0x926af0,
        artillery_score_limit=0x956100,
        artillery_limit_scale=0xb44e4c,
        artillery_after_first_pass=0x9a0493,
        artillery_second_filter=0x9a04f0,
        artillery_second_accept=0x9a05d5, artillery_second_reject=0x9a0f5e,
        artillery_second_weight_continue=0x9a062d, artillery_second_weight_scale=0xb92378,
        vector_normalize=0x4a2a10, normalize_tolerance=0xb55588,
        artillery_second_aim_accept=0x9a06ac,
        artillery_second_position_continue=0x9a06f6,
        artillery_second_distance_continue=0x9a078e,
        artillery_second_weapons_accept=0x9a08b8,
        artillery_second_movement_continue=0x9a0981, artillery_second_movement_score=0x9a0bc2, artillery_second_movement_unit_score=0x9a0bba,
        artillery_second_driver_gate=0x9a0bc9, artillery_second_driver_accept=0x9a0c38, artillery_second_region=0x9a0c38, artillery_second_region_continue=0x9a0c7e,
        artillery_second_category_gate=0x9a0c7e, artillery_second_category_accept=0x9a0ca6, artillery_second_category_reject=0x9a0f5e,
        artillery_second_category_prefix=0x9a0ca6, artillery_second_category_factor=0x9a0cf0, artillery_second_category_factor_continue=0x9a0dba,
        artillery_second_category_score=0x9a0dba, artillery_second_category_score_continue=0x9a0f5e,
        artillery_second_iterator=0x9a0f5e, artillery_second_iterator_done=0x9a0f78,
        artillery_final_weights=0x9a0f8c, artillery_final_weights_continue=0x9a1001,
        artillery_parameter_manager=0xe0eb70,
        artillery_final_selection=0x9a1001, artillery_final_selection_bias=0xb95668,
        artillery_final_curve=0x9d6540,
        artillery_final_curve_table=0xe0f670,
        artillery_search_prefix=0x99f3cf, artillery_search_prefix_continue=0x99f6c3,
        artillery_candidate_setup=0x99f7ab, artillery_candidate_setup_continue=0x99f8d7,
        artillery_prefix_refresh_threshold=0xb7628c, artillery_prefix_context=0xe0ef54,
        artillery_prefix_query_manager=0xdfa8cc, artillery_prefix_query_seed=0xe0ee08,
        artillery_prefix_region_registry=0xe0edc0, artillery_prefix_tree_erase=0x5f2dc0,
        artillery_prefix_tree_advance=0x517e10,
        artillery_driver_reload=0x9a0bc2, artillery_allocation_reload=0x9a0f78,
        artillery_second_empty=0x9a0f7f,
        artillery_second_skip=0x9a0f8c,
        point_xz=0x97f070,
        point_in_symmetric_bounds=0x9d5cc0,
        region_contains_point2=0x964ac0,
        region_contains_point3=0x964b70,
        artillery_region_score=0x9a0031,
        artillery_region_continue=0x9a007a,
        artillery_region_scale=0xb55584,
        artillery_query_gate=0x99fe3b,
        artillery_query_accept=0x9a0031,
        artillery_query_scale=0xbf6328,
        component_event2_word=0x92db30,
        component_query_data=0x970440,
        artillery_query_default_data=0xe0ee60,
        artillery_query_interface_id=0xb77da0,
        query_vector_append=0x975ab0,
        query_vector_destroy=0x490610,
        artillery_query_vector_insert=0x972940,
        artillery_movement_gate=0x99fb31, artillery_movement_continue=0x99fc01,
        artillery_movement_scoring=0x99fc01, artillery_movement_delta_limit=0xb5bdf8, artillery_movement_quarter=0xb55318,
        vector_cross_assign=0x6eb030, component_event2_scalar=0x970550,
        artillery_movement_score=0x99fe3b, artillery_movement_unit_score=0x99fe33,
        artillery_movement_scale=0xb44e48, artillery_movement_alt_scale=0xb69400, artillery_movement_threshold=0xb5dc9c,
        artillery_aim_gate=0x99fa61, artillery_aim_gate_accept=0x99fb31, artillery_weight_scale=0xb55580,
        transform_point=0x49b340, vector_difference=0x49b490, aim_world_position=0x9be440, component_position=0x994a80,
        artillery_weapons=0x99f8d7, artillery_weapons_accept=0x99fa61, artillery_weapon_bias=0xb5b7e4,
        tree_previous=0x68ee10, tree_rotate_left=0x996f10, tree_rotate_right=0x66b6c0, tree_construct=0x517c10, history_insert_node=0x99ed10, tree_allocate_node=0x99ebe0, tree_string_assign=0x401ae0, tree_string_copy=0x4019f0, tree_exception_construct=0x7c2027, tree_logic_error_vtable=0xb427dc, tree_length_error_vtable=0xb427f4, tree_length_throw_info=0xc836a4, tree_length_message=0xb44270,
        aimer_track=0xa02a70, aimer_predict=0xa02bc0, aimer_projectile_offset=0xa02c30, aimer_solve=0xa02db0,
        aimer_trace_source=0xbfcb70, aimer_time_epsilon=0xb556dc, math_zero=0xb44444, math_half=0xb44e48,
        aimer_high_arc_angle=0xbfcbb0, aimer_step_scale=0xb7628c, aimer_step_bias=0xb5bdf8, aimer_pi=0xbfcb60,
        aimer_iteration_scale=0xb76560, aimer_iteration_bias=0xb555ac, aimer_angle_limit=0xb444bc,
        aimer_negative_angle_limit=0xb551ec, aimer_upper_angle=0xbfcbac, aimer_lower_angle=0xbfcba8,
        aimer_upper_angle_double=0xbfcbc0, aimer_lower_angle_double=0xbfcbb8, aimer_minimum_length_squared=0xb69400,
        strategic_same_cell=0x9cf470, strategic_cell_point=0x9d1330, strategic_link_z=0x9de9c0, strategic_link_x=0x9dea10, strategic_link_back_z=0x9dea60, strategic_link_back_x=0x9deaf0,
        astar_strategic_start=0x9de950, astar_strategic_goal=0x9de970, astar_strategic_estimate=0x9de990, astar_strategic_reached=0x9deb80, astar_strategic_within=0x9debd0, astar_strategic_step=0x9decf0, astar_strategic_expand=0x9deed0,
        strategic_x_mask=0xe0ed3c, strategic_z_mask=0xe0ed2c, ai_indexed_object=0x980120,
        astar_strategic_run=0x9cf5c0, search_notify_begin=0x9def90, search_notify_end=0x9defd0, search_notify_finish=0x9df220, astar_engine=0x9e4620, unsigned_bias=0xb42f8c,
        astar_strategic_setup=0x9dec20, bitset_init=0x9ceda0, strategic_point_query=0xa07350, astar_reach_scale=0xb5bdf8,
        task_sleep=0x9de050, task_tree_remove=0xa09e90, scheduler_time_left=0xa09f50, scheduler_add=0xa09f70, scheduler_remove=0xa0a000,
        task_minus_one=0xb55068, get_time_seconds=0x894540,
        task_local_search_execute=0x9de160, task_strategic_search_execute=0x9de230, local_search_current=0xe0f9cc, strategic_search_current=0xe0f9d0,
        local_searcher=0xe10f98, strategic_searcher=0xe11860, searcher_step=0xa09340, searcher_run=0xa09880, search_unlimited_time=0xbfccf0, searcher_deliver=0xa098b0, path_deque_push_back=0xa09010, path_deque_destroy=0x554100,
        astar_local_estimate=0x9e5180, astar_local_reached=0x9e63f0, astar_local_set_start=0x9e5da0, astar_local_set_goal=0x9e5e10, round_to_int=0x522cf0, local_layer_at=0x9e57f0, local_terrain=0xda21a0, astar_local_add_cell=0x9e6ee0, astar_local_add_cell_avoiding=0x9e7200, astar_local_successors=0x9e7b80, local_world_mask_x=0xe0ed3c, local_world_mask_z=0xe0ed2c, searcher_path=0xa08ad0, task_zero=0xb426c0,
        task_unschedule=0x9de020, task_queue=0x9de030, task_player_map_execute=0x9de300, task_strategic_player_map_execute=0x9de390,
        local_search_prepare=0x9e55d0, local_search_finish=0x9e61e0, strategic_map_prepare=0x9e56e0,
        local_search_task_name=0xbf97dc, strategic_search_task_name=0xbf97f4,
        cell_query_task_name=0xbf97c0, strategic_player_map_task_name=0xbf9810,
        task_strategic_search_run=0x9de410, cell_query_init=0x9de100, strategic_reach_limit=0xb90220,
        task_cell_query_execute=0x9de710, strategic_layer_point=0x9d1330, strategic_ground_height=0x9e58b0,
        strategic_cell_distance=0x9de5c0, player_map_search_init=0x9de670,
        search_timer=0xe0ef50, search_log=0xe0ee08, searcher_reset=0xa09190, searcher_alloc=0xa092c0,
        searcher_heap_push=0xa09090, searcher_heap_pop=0xa08a60, searcher_heap_update=0xa08e70, searcher_unclose=0xa089e0,
        searcher_sift_down=0xa08870, searcher_push_back=0xa08fc0, search_random=0x4e0990, search_random_state=0xd30c00,
        search_tie_scale=0xb5d414, search_tie_half=0xb44e48, searcher_node_pool=0xe11820, searcher_pool_free=0x412fc0, searcher_resize=0xa08de0,
        path_ring_extend=0xa08440, searcher_path_to_ring=0xa08b50, searcher_closed_points=0xa08c60,
        ftol2=0x7c2dd0, finite=0x7c51fb,
        aim_direction=0x99eae0, aim_within_limits=0x9be880, aim_compose=0x49acd0,
        aim_inverse_sine=0x516f80, aim_negative_one=0xb5456c, aim_pi=0xbf7e90,
        float_minimum=0x422f30, float_maximum=0x422f90, float_clamp=0x4a94a0,
        vector_length=0x49aac0, vector_divide=0x5184c0, math_one=0xb4456c,
        pool=0xe0cd58, trace_index=0xdfa918, trace_mask=0xd1a088, trace_files=0xdfa8f8,
        trace_lines=0xdfa8d8, trace_source=0xbf7e38, context_begin=0x9dce60,
        context_end=0x9dcc10, context_record=0xa64d40, native_bailout=0x984c20,
        native_vehicle=0x9856f0, artillery=0x9a13a0, artillery_score=0x99f2a0, target_history=0x99f040, target_history_insert=0x99ef80, artillery_filter=0x99f6c3, artillery_filter_accept=0x99f7ab, artillery_filter_reject=0x9a0476, artillery_filter_zero=0xb44444, artillery_filter_history_limit=0xbf62e8, artillery_component_view=0x49a980, interpreter=0x9be180,
        bailout_curve=0x9d7200, bailout_score=0x9d6b90,
        bailout_curve_table=0xe0f678, bailout_score_table=0xe0f674, bailout_pattern=0x983280,
        bailout_curve_initializer=0x9d6c20, bailout_score_initializer=0x9d65b0, curve_allocator=0x403610,
        collision=0x9d4ba0, collision_body_view=0x66b190, collision_interface_id=0xb77ee0, collision_handle_field=0x164,
        collision_registry=0xd7d01c, collision_actors=0xdfa8c8, collision_clock=0xe0ef50,
        collision_pool_entry=0x926af0, collision_event_interface=0x92afb0,
        collision_allocate=0x412ee0, collision_allocator=0xe0f7c0, collision_alloc_source=0xb44284,
        vector_allocator=0x403610, vector_free=0x403680, vector_length_error=0xa894e0,
        vector_copy=0xa6ad10, vector_fill=0x4ef090, vector_shift=0xa03770, vector_fill_range=0x6f7000,
        vector_memmove=0x7c16f0, vector_frame_handler=0x7c1b6e, vector_throw=0x7c1fae, vector_original_handler=0xac4a20,
        collision_event_manager=0xe0eb20, collision_vector_insert=0x428200, collision_construct=0x9dc3b0, collision_notify=0x9d4b60,
        collision_dispatch=0x9d48b0, collision_notify_state_field=0x1ec,
        collision_line_distance=0x9d4640, collision_distance=0x9d46d0, collision_distance_limit=0xb76560,
        patches=[(0xbf7e34,0x9be180,'bfv_interpret'),(0xbf5a18,0x984c20,'bfv_bailout'),(0xbf5ab8,0x9856f0,'bfv_vehicle'),(0xbf8c64,0x9d4ba0,'bfv_collision'),(0xbf6378,0x9a13a0,'bfv_artillery')]),
    'server': dict(file='bfvietnam_w32ded.exe', output='bfvietnam_w32ded-editable.exe',
        sha='86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d',
        list_find_word=0x775bf0,
        artillery_category_score=0x74a85a,
        artillery_category_alternate=0x74a9d9,
        artillery_category_scale=0x81df08,
        target_handle_eligible=0x728c90,
        artillery_alternate_gate=0x74a9d9,
        artillery_alternate_accept=0x74aa44,
        artillery_nested_finish=0x74abac,
        artillery_next_target=0x71b110,
        artillery_lookup_target=0x6e8930,
        artillery_score_limit=0x713230,
        artillery_limit_scale=0x873bf0,
        artillery_after_first_pass=0x74ac73,
        artillery_second_filter=0x74acd0,
        artillery_second_accept=0x74adb5, artillery_second_reject=0x74b73e,
        artillery_second_weight_continue=0x74ae0d, artillery_second_weight_scale=0x8372b4,
        vector_normalize=0x438d30, normalize_tolerance=0x80b630,
        artillery_second_aim_accept=0x74ae8c,
        artillery_second_position_continue=0x74aed6,
        artillery_second_distance_continue=0x74af6e,
        artillery_second_weapons_accept=0x74b098,
        artillery_second_movement_continue=0x74b161, artillery_second_movement_score=0x74b3a2, artillery_second_movement_unit_score=0x74b39a,
        artillery_second_driver_gate=0x74b3a9, artillery_second_driver_accept=0x74b418, artillery_second_region=0x74b418, artillery_second_region_continue=0x74b45e,
        artillery_second_category_gate=0x74b45e, artillery_second_category_accept=0x74b486, artillery_second_category_reject=0x74b73e,
        artillery_second_category_prefix=0x74b486, artillery_second_category_factor=0x74b4d0, artillery_second_category_factor_continue=0x74b59a,
        artillery_second_category_score=0x74b59a, artillery_second_category_score_continue=0x74b73e,
        artillery_second_iterator=0x74b73e, artillery_second_iterator_done=0x74b758,
        artillery_final_weights=0x74b76c, artillery_final_weights_continue=0x74b7e1,
        artillery_parameter_manager=0xc31178,
        artillery_final_selection=0x74b7e1, artillery_final_selection_bias=0x839ac0,
        artillery_final_curve=0x78b930,
        artillery_final_curve_table=0xc31c08,
        artillery_search_prefix=0x749baf, artillery_search_prefix_continue=0x749ea3,
        artillery_candidate_setup=0x749f8b, artillery_candidate_setup_continue=0x74a0b7,
        artillery_prefix_refresh_threshold=0x81e9cc, artillery_prefix_context=0xc314b4,
        artillery_prefix_query_manager=0xc1d338, artillery_prefix_query_seed=0xc313d0,
        artillery_prefix_region_registry=0xc31388, artillery_prefix_tree_erase=0x5c7980,
        artillery_prefix_tree_advance=0x718e10,
        artillery_driver_reload=0x74b3a2, artillery_allocation_reload=0x74b758,
        artillery_second_empty=0x74b75f,
        artillery_second_skip=0x74b76c,
        point_xz=0x728f20,
        point_in_symmetric_bounds=0x78cd10,
        region_contains_point2=0x71c280,
        region_contains_point3=0x71c330,
        artillery_region_score=0x74a811,
        artillery_region_continue=0x74a85a,
        artillery_region_scale=0x81fbf4,
        artillery_query_gate=0x74a61b,
        artillery_query_accept=0x74a811,
        artillery_query_scale=0x874728,
        component_event2_word=0x6eb4b0,
        component_query_data=0x727d50,
        artillery_query_default_data=0xc31428,
        artillery_query_interface_id=0x820658,
        query_vector_append=0x72d870,
        query_vector_destroy=0x6ff4f0,
        artillery_query_vector_insert=0x72d640,
        artillery_movement_gate=0x74a311, artillery_movement_continue=0x74a3e1,
        artillery_movement_scoring=0x74a3e1, artillery_movement_delta_limit=0x8220c8, artillery_movement_quarter=0x81fb68,
        vector_cross_assign=0x611430, component_event2_scalar=0x72e7d0,
        artillery_movement_score=0x74a61b, artillery_movement_unit_score=0x74a613,
        artillery_movement_scale=0x8148e0, artillery_movement_alt_scale=0x8377b4, artillery_movement_threshold=0x814d1c,
        artillery_aim_gate=0x74a241, artillery_aim_gate_accept=0x74a311, artillery_weight_scale=0x81fac0,
        transform_point=0x5b25f0, vector_difference=0x48d160, aim_world_position=0x78df90, component_position=0x73f1e0,
        artillery_weapons=0x74a0b7, artillery_weapons_accept=0x74a241, artillery_weapon_bias=0x81fac4,
        tree_previous=0x73f240, tree_rotate_left=0x5c7460, tree_rotate_right=0x55c080, tree_construct=0x7686e0, history_insert_node=0x7494f0, tree_allocate_node=0x7493c0, tree_string_assign=0x402020, tree_string_copy=0x401e20, tree_exception_construct=0x63aa5e, tree_logic_error_vtable=0x80737c, tree_length_error_vtable=0x807388, tree_length_throw_info=0x8ce900, tree_length_message=0x808f70,
        aimer_track=0x7b7090, aimer_predict=0x7b71e0, aimer_projectile_offset=0x7b7250, aimer_solve=0x7b73d0,
        aimer_trace_source=0x87b4b0, aimer_time_epsilon=0x817cf4, math_zero=0x851bcc, math_half=0x8148e0,
        aimer_high_arc_angle=0x87b4f0, aimer_step_scale=0x81e9cc, aimer_step_bias=0x8220c8, aimer_pi=0x87b4a0,
        aimer_iteration_scale=0x81facc, aimer_iteration_bias=0x81fac8, aimer_angle_limit=0x8091b4,
        aimer_negative_angle_limit=0x80b660, aimer_upper_angle=0x87b4ec, aimer_lower_angle=0x87b4e8,
        aimer_upper_angle_double=0x87b500, aimer_lower_angle_double=0x87b4f8, aimer_minimum_length_squared=0x8377b4,
        strategic_same_cell=0x784e60, strategic_cell_point=0x786b30, strategic_link_z=0x792620, strategic_link_x=0x792670, strategic_link_back_z=0x7926c0, strategic_link_back_x=0x792750,
        astar_strategic_start=0x7925b0, astar_strategic_goal=0x7925d0, astar_strategic_estimate=0x7925f0, astar_strategic_reached=0x7927e0, astar_strategic_within=0x792830, astar_strategic_step=0x792950, astar_strategic_expand=0x792b30,
        strategic_x_mask=0xc314d0, strategic_z_mask=0xc314c0, ai_indexed_object=0x72a080,
        astar_strategic_run=0x784fb0, search_notify_begin=0x792bf0, search_notify_end=0x792c30, search_notify_finish=0x792e80, astar_engine=0x798280, unsigned_bias=0x807c7c,
        astar_strategic_setup=0x792880, bitset_init=0x784590, strategic_point_query=0x7bb930, astar_reach_scale=0x8220c8,
        task_sleep=0x791cb0, task_tree_remove=0x7bebd0, scheduler_time_left=0x7bec90, scheduler_add=0x7becb0, scheduler_remove=0x7bed40,
        task_minus_one=0x814ac8, get_time_seconds=0x6634b0,
        task_local_search_execute=0x791dc0, task_strategic_search_execute=0x791e90, local_search_current=0xc31d98, strategic_search_current=0xc31d9c,
        local_searcher=0xc334e0, strategic_searcher=0xc33da8, searcher_step=0x7bd990, searcher_run=0x7bded0, search_unlimited_time=0x87b668, searcher_deliver=0x7bdf00, path_deque_push_back=0x68b240, path_deque_destroy=0x768f00,
        astar_local_estimate=0x798e10, astar_local_reached=0x79a080, astar_local_set_start=0x799a30, astar_local_set_goal=0x799aa0, round_to_int=0x4d8260, local_layer_at=0x799480, local_terrain=0xc0f118, astar_local_add_cell=0x79ab70, astar_local_add_cell_avoiding=0x79ae90, astar_local_successors=0x79b810, local_world_mask_x=0xc314d0, local_world_mask_z=0xc314c0, searcher_path=0x7bd040, task_zero=0x807350,
        task_unschedule=0x791c80, task_queue=0x791c90, task_player_map_execute=0x791f60, task_strategic_player_map_execute=0x791ff0,
        local_search_prepare=0x799260, local_search_finish=0x799e70, strategic_map_prepare=0x799370,
        local_search_task_name=0x877e3c, strategic_search_task_name=0x877e54,
        cell_query_task_name=0x877e20, strategic_player_map_task_name=0x877e70,
        task_strategic_search_run=0x792070, cell_query_init=0x791d60, strategic_reach_limit=0x836acc,
        task_cell_query_execute=0x792370, strategic_layer_point=0x786b30, strategic_ground_height=0x799540,
        strategic_cell_distance=0x792220, player_map_search_init=0x7922d0,
        search_timer=0xc314b0, search_log=0xc313d0, searcher_reset=0x7bd7e0, searcher_alloc=0x7bd910,
        searcher_heap_push=0x7bd6e0, searcher_heap_pop=0x7bcfd0, searcher_heap_update=0x7bd540, searcher_unclose=0x7bcf50,
        searcher_sift_down=0x7bcde0, searcher_push_back=0x7bd690, search_random=0x4b1fd0, search_random_state=0x9436a0,
        search_tie_scale=0x81df34, search_tie_half=0x8148e0, searcher_node_pool=0xc33d68, searcher_pool_free=0x413600, searcher_resize=0x7bd350,
        path_ring_extend=0x7bca20, searcher_path_to_ring=0x7bd0c0, searcher_closed_points=0x7bd1d0,
        ftol2=0x63c530, finite=0x63db18,
        aim_direction=0x7492c0, aim_within_limits=0x78e3d0, aim_compose=0x438590,
        aim_inverse_sine=0x48b570, aim_negative_one=0x817648, aim_pi=0x877770,
        float_minimum=0x5a3340, float_maximum=0x4ac530, float_clamp=0x6f0470,
        vector_length=0x48c2b0, vector_divide=0x48caf0, math_one=0x809264,
        pool=0xc2f7c0, trace_index=0xc1d380, trace_mask=0x934448, trace_files=0xc1d360,
        trace_lines=0xc1d340, trace_source=0x876820, context_begin=0x7b6bd0,
        context_end=0x7b6980, context_record=0x69f0b0, native_bailout=0x72eef0,
        native_vehicle=0x72f9e0, artillery=0x74bb80, artillery_score=0x749a80, target_history=0x749820, target_history_insert=0x749760, artillery_filter=0x749ea3, artillery_filter_accept=0x749f8b, artillery_filter_reject=0x74ac56, artillery_filter_zero=0x851bcc, artillery_filter_history_limit=0x8746e8, artillery_component_view=0x437e80, interpreter=0x774ff0,
        bailout_curve=0x78c5f0, bailout_score=0x78bf80,
        bailout_curve_table=0xc31c10, bailout_score_table=0xc31c0c, bailout_pattern=0x72e510,
        bailout_curve_initializer=0x78c010, bailout_score_initializer=0x78b9a0, curve_allocator=0x403d80,
        collision=0x78a500, collision_body_view=0x564460, collision_interface_id=0x820798, collision_handle_field=0x15c,
        collision_registry=0xbecc64, collision_actors=0xc1d334, collision_clock=0xc314b0,
        collision_pool_entry=0x6e8930, collision_event_interface=0x6e88b0,
        collision_allocate=0x404290, collision_allocator=0xc33388, collision_alloc_source=0x807375,
        vector_allocator=0x403d80, vector_free=0x403df0, vector_length_error=0x48e690,
        vector_copy=0x41fbd0, vector_fill=0x5f83e0, vector_shift=0x7901d0, vector_fill_range=0x705df0,
        vector_memmove=0x63a0d0, vector_frame_handler=0x63a54e, vector_throw=0x63a98e, vector_original_handler=0x7e1080,
        collision_event_manager=0xc3117c, collision_vector_insert=0x4297c0, collision_construct=0x7ae550, collision_notify=0x78a4c0,
        collision_dispatch=0x78a210, collision_notify_state_field=0x1c0,
        collision_line_distance=0x789fa0, collision_distance=0x78a030, collision_distance_limit=0x81facc,
        patches=[(0x87681c,0x774ff0,'bfv_interpret'),(0x873d88,0x72eef0,'bfv_bailout'),(0x873e28,0x72f9e0,'bfv_vehicle'),(0x8775cc,0x78a500,'bfv_collision'),(0x874778,0x74bb80,'bfv_artillery')]),
}


def align(n, size): return (n + size - 1) // size * size


def run(args, cwd):
    result = subprocess.run([str(x) for x in args], cwd=cwd, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(f'Command failed: {args}\n{result.stdout}\n{result.stderr}')
    return result.stdout


# the hand-written modules (src/*.c); tools/linkmode.py compiles them the same way
SRC_FLAGS=['-m32','-std=c11','-O2','-Wall','-Wextra','-Werror','-ffreestanding','-fno-builtin','-frounding-math',
           '-fno-stack-protector','-fno-asynchronous-unwind-tables','-fno-unwind-tables']
LIFTED_FLAGS=['-m32','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-array-bounds','-ffreestanding','-fno-builtin',
    '-frounding-math','-fno-delete-null-pointer-checks','-fno-stack-protector','-fno-asynchronous-unwind-tables','-fno-unwind-tables','-mgeneral-regs-only']

# The lifted tree the build detours into: lifted/ (L1) or readable/ (L2, the
# same functions and symbols written as readable C; tools/lift_l2.py).
TREE='lifted'
MODS_ONLY=False  # --mods-only: only the functions mods hook run as C

def tree_runtime():
    """Runtime files hashed by verify_lifted.runtime_sha256, in its order."""
    runtime=PROJECT/TREE/'runtime'
    return [h for h in (runtime/'l1.h',runtime/'l2.h',runtime/'symbols_client.h',runtime/'symbols_server.h')
            if h.name=='l1.h' or h.exists()]+[runtime/'l1_runtime.c']

def lifted_sources(target):
    lifted=PROJECT/TREE
    return [lifted/'runtime/l1_runtime.c',*sorted((lifted/target).rglob('*.c'))]

def code_sha256(text):
    """Hash of a lifted function's code without comments or layout: names and
    notes in comments (sort_lifted.py) are not edits."""
    import re
    return hashlib.sha256(' '.join(re.sub(r'/\*.*?\*/',' ',text,flags=re.S).split()).encode()).hexdigest()

def edited(entry,text):
    """True when a lifted function's code differs from what lift_c.py generated."""
    if 'code_sha256' in entry:return code_sha256(text)!=entry['code_sha256']
    return hashlib.sha256(text.encode()).hexdigest()!=entry['c_sha256']

def response(path,files):
    """@file argument listing files (the lifted objects exceed the Windows command-line limit)."""
    path.write_text(''.join('"'+Path(f).as_posix()+'"\n' for f in files))
    return '@'+str(path)

def compile_lifted(target,work):
    """Compile the lifted C tree (all of it; only verified entries get detoured).

    Objects are kept in build/<target>/l1|l2 (shared by --output builds) next to
    a .key file: the hash of the compiler, flags, every runtime file and the
    source. A source whose key is unchanged is not compiled again."""
    from concurrent.futures import ThreadPoolExecutor
    sources=lifted_sources(target)
    store=PROJECT/'build'/target/('l1' if TREE=='lifted' else 'l2')
    objects=[store/(s.relative_to(PROJECT/TREE).with_suffix('.o')) for s in sources]
    for o in objects:o.parent.mkdir(parents=True,exist_ok=True)
    common=hashlib.sha256(str([str(COMPILERS/'gcc.exe'),*LIFTED_FLAGS]).encode())
    for f in sorted((PROJECT/TREE/'runtime').iterdir()):
        if f.is_file():common.update(f.name.encode()+bytes(1)+f.read_bytes())
    def build_one(so):
        source,o=so
        h=common.copy();h.update(source.read_bytes());key=h.hexdigest()
        stamp=o.with_suffix('.key')
        if o.exists() and stamp.exists() and stamp.read_text()==key:return 0
        partial=o.with_suffix(f'.{os.getpid()}.tmp')
        run([COMPILERS/'gcc.exe',*LIFTED_FLAGS,'-I',PROJECT/TREE/'runtime','-c',source,'-o',partial],work)
        os.replace(partial,o);stamp.write_text(key)
        return 1
    with ThreadPoolExecutor(max(6,os.cpu_count() or 6)) as pool:
        compiled=sum(pool.map(build_one,zip(sources,objects)))
    print(f'  {target}: lifted C {compiled} of {len(sources)} file(s) compiled ({len(sources)-compiled} unchanged)')
    return objects

def array_words(data):
    from array import array
    return array('I',data[:len(data)//4*4])

def lifted_entries(target,spec,pe,original,hand_entries):
    """Detours for lifted functions that passed verify_lifted.py on exactly these sources.

    An entry is skipped when hand-written C already owns it, when it is not an
    instruction start of the linear .text decode that static_recomp reassembles
    byte-exact (a misaligned index entry would cut a native instruction in half),
    when anything may enter its first five bytes other than at the entry (a
    labelled branch target, an image dword pointing there, or another lifted
    function's exit), or when its detour would overlap another patch."""
    lifted=PROJECT/TREE/target
    if not (lifted/'catalog.json').exists():return [],{}
    # The full verifier report stays in build/; verified.json is its committed
    # summary (passed functions only), so a fresh clone builds the same detours.
    report_path=PROJECT/'build'/target/f'{TREE}-verification.json'
    summary_path=lifted/'verified.json'
    catalog=json.loads((lifted/'catalog.json').read_text())
    if report_path.exists():
        report=json.loads(report_path.read_text())
        summary={k:report[k] for k in ('original_sha256','runtime_sha256','verifier')}
        summary['passed']={r['address']:r['c_sha256'] for r in report['results'] if r['status']=='passed' and r.get('c_sha256')}
        text=json.dumps(summary,indent=0,sort_keys=True)+'\n'
        if not summary_path.exists() or summary_path.read_text()!=text:summary_path.write_text(text,newline='\n')
    elif summary_path.exists():
        summary=json.loads(summary_path.read_text())
        report=dict(summary,results=[dict(address=a,c_sha256=s,status='passed') for a,s in summary['passed'].items()])
    else:return [],dict(not_verified=1)
    if catalog['original_sha256']!=spec['sha'] or report['original_sha256']!=spec['sha']:
        raise RuntimeError('Lifted catalog or verification hash differs from the ABI target')
    runtime=hashlib.sha256()  # same hash as verify_lifted.runtime_sha256
    for f in tree_runtime():runtime.update(f.name.encode()+bytes(1)+f.read_bytes())
    if report.get('runtime_sha256')!=runtime.hexdigest():
        raise RuntimeError(f'{target}: lifted runtime changed since verify_lifted.py; rerun it (or remove lifted-verification.json)')
    # only functions whose current text is exactly the verified text
    passed={(r['address'],r['c_sha256']) for r in report['results'] if r['status']=='passed' and r.get('c_sha256')}
    entries=[e for e in catalog['entries'] if (e['address'],e.get('c_sha256')) in passed]
    # Mods: a verified function whose text was edited still runs (as edited);
    # an edited function that never passed verification stays native.
    from sort_lifted import split_texts
    texts=split_texts(lifted);ok={e['address'] for e in entries}
    for e in catalog['entries']:
        if edited(e,texts.get(e['symbol'],'')):
            print(f'  {target}: edited lifted function {e["address"]}'+('' if e['address'] in ok else ' (never verified: stays native)'))
    interior={}
    for e in entries:
        a=int(e['address'],16)
        for x in range(a+1,a+5):interior[x]=e
    blocked=set();first_hit={}  # entry -> lowest interior offset something enters
    def block_at(e,x):
        blocked.add(e['address'])
        first_hit[e['address']]=min(first_hit.get(e['address'],5),x-int(e['address'],16))
    from capstone import Cs,CS_ARCH_X86,CS_MODE_32  # same linear sweep as static_recomp.TextEmitter.plan
    text=next(x for x in pe.sections if x.Name.rstrip(b'\0')==b'.text')
    code=text.get_data()[:text.Misc_VirtualSize]
    base=pe.OPTIONAL_HEADER.ImageBase+text.VirtualAddress
    # Image pointers: an absolute immediate or displacement field of a .text
    # instruction, a 4-aligned .text dword next to another .text address (jump
    # table), or any 4-aligned dword of another section. Other byte windows that
    # happen to spell an interior address are not pointers.
    starts=set();sizes={};offset=0;md=Cs(CS_ARCH_X86,CS_MODE_32);md.detail=True
    sources={}  # direct branch target -> source addresses
    # Code pointers land on instruction starts: a pointer-like dword hits an
    # entry only when it names one of the instructions inside its first five
    # bytes (branches and exits are checked against every interior byte).
    pointed={}
    for e in entries:
        a=int(e['address'],16)
        for i in md.disasm(pe.get_data(a-pe.OPTIONAL_HEADER.ImageBase,20),a):
            if i.address>=a+5:break
            if i.address>a:pointed[i.address]=e
    code_refs=set()
    def hit(v):
        code_refs.add(v)
        if v in pointed:block_at(pointed[v],v)
    while offset<len(code):
        last=offset
        for i in md.disasm(code[offset:],base+offset):
            starts.add(i.address);sizes[i.address]=i.size;last=i.address-base+i.size
            branch=(i.group(1) or i.group(2)) and i.op_str.startswith('0x') and ',' not in i.op_str and ':' not in i.op_str  # direct jump, jcc, loop or call
            if branch:
                sources.setdefault(int(i.op_str,16),[]).append(i.address)
                continue  # its immediate is relative
            for o in (i.disp_offset,i.imm_offset):
                if o and o+4<=i.size:hit(int.from_bytes(i.bytes[o:o+4],'little'))
        offset=last if last>offset else offset+1
    # An entry the linear sweep decodes through (usually right after a jump table)
    # is still a real entry when another indexed function calls or jumps to it
    # directly and nothing branches to the sweep instruction that covers it.
    import bisect
    spans=sorted(tuple(int(y,16) for y in r.split('-')) for e in catalog['entries'] for r in e['ranges']);span_starts=[lo for lo,_ in spans]
    def indexed(x):
        k=bisect.bisect_right(span_starts,x)-1
        return k>=0 and spans[k][0]<=x<spans[k][1]
    # Or when a vtable slot or a data dword points at it and the sweep instruction
    # covering it lies in no indexed function (jump-table data decoded as code).
    # A covering instruction inside another function means the index entry is
    # not a function start (server 0x757063 inside 0x7559f0's `lea`).
    vtable_targets={int(r.split('\t')[2],16) for r in (PROJECT/'lifted'/target/'VTABLES.tsv').read_text().splitlines()[1:]} \
        if (PROJECT/'lifted'/target/'VTABLES.tsv').exists() else set()
    data_words=set()
    for section in pe.sections:
        if section is not text:data_words.update(array_words(section.get_data()))
    def real_entry(e):
        a=int(e['address'],16);own=[tuple(int(y,16) for y in r.split('-')) for r in e['ranges']]
        if any(p in sources for p in range(a-15,a) if p in starts):return False
        if any(indexed(s) and not any(lo<=s<hi for lo,hi in own) for s in sources.get(a,())):return True
        cover=[p for p in range(a-15,a) if p in sizes and p+sizes[p]>a]
        return bool(cover) and (a in vtable_targets or a in data_words) and \
            not any(indexed(p) and not any(lo<=p<hi for lo,hi in own) for p in cover)
    misaligned={e['address'] for e in entries if int(e['address'],16) not in starts and not real_entry(e)}
    stats_misaligned_entries=sum(1 for e in entries if int(e['address'],16) not in starts)-len(misaligned)
    blocked|=misaligned
    from array import array
    words=array('I',code[:len(code)//4*4]);assert base%4==0
    for k,v in enumerate(words):
        if v in interior and ((k and base<=words[k-1]<base+len(code)) or (k+1<len(words) and base<=words[k+1]<base+len(code))):hit(v)
    for section in pe.sections:
        if section is text:continue
        data=section.get_data()
        for v in set(array('I',data[:len(data)//4*4])).intersection(interior):hit(v)
    # A labelled interior address is harmless when every branch to it is a direct
    # branch inside the same lifted function (the C has its own goto for it) and
    # that function has no indirect jump that could land there natively.
    def own(e,x):
        spans=[tuple(int(y,16) for y in r.split('-')) for r in e['ranges']]
        if not sources.get(x) or not all(any(lo<=s<hi for lo,hi in spans) for s in sources[x]):return False
        for lo,hi in spans:
            for i in md.disasm(code[lo-base:hi-base],lo):
                if i.mnemonic=='jmp' and not i.op_str.startswith('0x'):return False
        return True
    labels_file=PROJECT/'build'/f'{target}-recomp'/'text.s'
    if not labels_file.exists():raise RuntimeError(f'{target}: run recomp.ps1 once first (branch labels are needed for the detour audit)')
    labels=set()
    with labels_file.open(encoding='utf-8') as f:
        for line in f:
            if line[:4] in ('loc_','sub_') and line.rstrip().endswith(':'):
                x=int(line[4:12],16);labels.add(x)
                if x in interior and not own(interior[x],x):block_at(interior[x],x)
    for x,e in interior.items():
        if x in sources and not own(e,x):block_at(e,x)
    for e in catalog['entries']:
        for t in e['exits']:
            x=int(t,16)
            if x in interior:block_at(interior[x],x)
    stats=dict(passed=len(entries),misaligned=len(misaligned),resynced=stats_misaligned_entries,interior_target=len(blocked-misaligned))
    # verify_live_exceptions.py patches the call in the original vector-insert catch
    # funclet, so that funclet stays native
    u32=lambda a:int.from_bytes(pe.get_data(a-pe.OPTIONAL_HEADER.ImageBase,4),'little')
    funclet=u32(u32(u32(u32(spec['vector_original_handler']+1)+16)+16)+12)
    instrumented={e['address'] for e in entries if any(int(r.split('-')[0],16)<=funclet<int(r.split('-')[1],16) for r in e['ranges'])}
    blocked|=instrumented;stats.update(instrumented=len(instrumented))
    # hand-written C wins: skip lifted functions that contain any of its patches
    hand=sorted(location for location,_,_ in hand_entries)
    import bisect
    # An entry that is entered only at +2..+4 takes a two-byte `jmp short` to a
    # five-byte jump placed in int3 padding within 127 bytes. The padding must lie
    # outside every indexed function, be referenced by nothing (branch, label,
    # operand or data dword) and overlap no other patch; the original bytes from
    # +2 on stay as they were for the native paths that enter there.
    fspans=[]
    for row in (PROJECT.parent/'reports'/target/'all-functions.tsv').read_text(encoding='utf-8').splitlines()[1:]:
        f=row.split('\t');fspans.append((int(f[0],16),int(f[0],16)+int(f[2])))
    fspans=sorted(fspans+spans);fstarts=[lo for lo,_ in fspans];fends=[]
    for lo,hi in fspans:fends.append(max(hi,fends[-1]) if fends else hi)  # running maximum end
    def in_function(x):
        k=bisect.bisect_right(fstarts,x)-1
        return k>=0 and fends[k]>x
    used=set()
    for location,expected,_ in hand_entries:used.update(range(location,location+len(expected)))
    for e in entries:
        if e['address'] not in blocked:
            a=int(e['address'],16);used.update(range(a,a+len(bytes.fromhex(e['guarded_prefix']))))
    referenced=set(sources)|labels|code_refs|data_words
    def padding(a):
        for t in sorted(range(a+2-128,a+2+124),key=lambda t:abs(t-a)):
            run=range(t,t+5)
            if not all(base<=x<base+len(code) and code[x-base]==0xcc for x in run):continue
            if any(x in used or x in referenced or in_function(x) for x in run):continue
            return t
    trampolines={}
    for e in entries:
        x=e['address']
        if x not in blocked or x in misaligned or x in instrumented or first_hit.get(x,0)<2:continue
        a=int(x,16);t=padding(a)
        if t is None:continue
        trampolines[a]=t;used.update(range(t,t+5));blocked.discard(x)
    result=[];skipped_taken=0
    for e in entries:
        a=int(e['address'],16)
        if e['address'] in blocked:continue
        spans=[tuple(int(x,16) for x in r.split('-')) for r in e['ranges']]
        if any(bisect.bisect_left(hand,lo-15)<bisect.bisect_left(hand,hi) for lo,hi in spans):
            skipped_taken+=1;trampolines.pop(a,None);continue
        prefix=bytes.fromhex(e['guarded_prefix'])
        if a in trampolines:prefix=prefix[:2]
        result.append((a,prefix,e['symbol']))
    stats.update(hand_written=skipped_taken,short_detours=len(trampolines),detoured=len(result))
    return result,stats,trampolines

def build(target,lifted=True,output_path=None):
    import mods
    if mods.ONLY is not None and 'vulkan' in mods.ONLY:
        if target != 'client': raise RuntimeError('vulkan is a client-only backend')
        if output_path is None: raise RuntimeError('vulkan requires --output BfVietnam-Vulkan.exe to preserve the original')
        if output_path.resolve() == (GAME / TARGETS[target]['file']).resolve():
            raise RuntimeError('vulkan output must not replace the original client')
    spec = TARGETS[target]
    original = (GAME / spec['file']).read_bytes()
    if hashlib.sha256(original).hexdigest() != spec['sha']:
        raise RuntimeError('Original image hash differs from the verified ABI target')
    pe = pefile.PE(data=original)
    if pe.OPTIONAL_HEADER.ImageBase != 0x400000 or pe.OPTIONAL_HEADER.DllCharacteristics & 0x40:
        raise RuntimeError('Only the inspected fixed-base executable layout is supported')
    # another output (tests) gets its own work folder, so build/<target> keeps describing the game build
    work = PROJECT / 'build' / (target if output_path is None else target+'-custom')
    work.mkdir(parents=True, exist_ok=True)
    constants = {('OBJECT_POOL' if key == 'pool' else key.upper()): value
        for key, value in spec.items() if isinstance(value, int)}
    target_flags=f'#define BFV_BUILD_CLIENT {int(target=="client")}\n#define BFV_BUILD_SERVER {int(target=="server")}\n'
    (work / 'target.h').write_text(target_flags+''.join(f'#define BFV_{k} 0x{v:08x}u\n' for k,v in constants.items()))
    os.environ['PATH'] = str(COMPILERS) + os.pathsep + os.environ['PATH']
    priority={'native_ai':0,'mod_rules':1}
    sources = sorted((PROJECT/'src').glob('*.c'),key=lambda p:(priority.get(p.stem,2),p.name))
    objects = []
    for source_path in sources:
        source = source_path.stem
        run([COMPILERS/'gcc.exe',*SRC_FLAGS,'-I',work,'-c',source_path,'-o',work/f'{source}.o'], work)
        objects.append(work/f'{source}.o')
    archives=[]
    if lifted and (PROJECT/TREE/target/'catalog.json').exists():
        lifted_objects=compile_lifted(target,work)
        if MODS_ONLY:
            # The lifted C goes in an archive: the linker takes only the files
            # the hooked functions are in (and the runtime).
            archive=work/'lifted.a'
            archive.unlink(missing_ok=True)
            run([COMPILERS/'ar.exe','rcs',archive,response(work/'archive.rsp',lifted_objects[1:])],work)
            objects.append(lifted_objects[0]);archives.append(archive)
        else:objects+=lifted_objects
    # mods/<target>/*.c: hooks around lifted functions (tools/mods.py)
    import mods
    mod_stubs,mod_files={},[]
    if mods.sources(target) and TREE!='readable':
        # The mod runtime calls game code through the L2 runtime (bfv_call, shadow
        # stack in readable/runtime/l1.h); lifted/runtime has neither.
        print(f'  {target}: mods left out: they need --tree readable',flush=True)
    elif MODS_ONLY and not mods.sources(target):raise RuntimeError(f'{target}: --mods-only but no mods')
    elif mods.sources(target):
        if not (lifted and (PROJECT/TREE/target/'catalog.json').exists()):raise RuntimeError(f'{target}: mods need the lifted C (drop --no-lifted)')
        mod_stubs,mod_files=mods.generate(target,work,PROJECT/TREE,pe,json.loads((PROJECT/TREE/target/'catalog.json').read_text()))
        if mod_files:objects+=mods.compile(target,work,PROJECT/TREE,mod_files,run,COMPILERS/'gcc.exe')
    objects+=archives  # after the mods, whose references pull files from it
    libgcc=[run([COMPILERS/'gcc.exe','-print-libgcc-file-name'],work).strip()] if mod_files else []
    rva = align(pe.OPTIONAL_HEADER.SizeOfImage, pe.OPTIONAL_HEADER.SectionAlignment)
    address = pe.OPTIONAL_HEADER.ImageBase + rva
    (work/'payload.ld').write_text(f'SECTIONS {{ . = 0x{address:x}; .payload : {{ *(.text*) *(.rdata*) *(.data*) *(.bss*) BYTE(0) }} .bfvshadow ALIGN(4096) (NOLOAD) : {{ *(.bfvshadow) }} /DISCARD/ : {{ *(.eh_frame*) *(.comment*) *(.drectve*) }} }}')
    run([COMPILERS/'ld.exe','-T',work/'payload.ld','--image-base','0x400000',
         '--entry','_bfv_interpret','--disable-dynamicbase','--disable-reloc-section',
         '-Map',work/'payload.map','-o',work/'payload.exe',response(work/'objects.rsp',objects),*libgcc],work)
    for archive in archives:archive.unlink()  # 80 MB copy of the objects, only needed by the link
    run([COMPILERS/'objcopy.exe','-O','binary','--only-section','.payload',
         work/'payload.exe',work/'payload.bin'],work)
    # The map retains final virtual addresses even with a raw binary output.
    import re
    symbols = {}
    for line in (work/'payload.map').read_text().splitlines():
        match = re.match(r'\s*(0x[0-9a-fA-F]+)\s+[_@]?(bfv_[A-Za-z0-9_]+)(?:@\d+)?\s*$',line)
        if match: symbols[match[2]] = int(match[1],16)
    payload = (work/'payload.bin').read_bytes()
    # the thread shadow stacks (readable runtime) are zero-initialized memory after the payload
    virtual_size = max(len(payload), symbols.get('bfv_shadow_end', address) - address)
    linked = pefile.PE(str(work/'payload.exe'))
    linked_section = next(s for s in linked.sections if s.Name.rstrip(b'\0') == b'.payload')
    if linked.OPTIONAL_HEADER.ImageBase + linked_section.VirtualAddress != address:
        raise RuntimeError('Linked payload address differs from the appended PE section')
    raw = align(len(original), pe.OPTIONAL_HEADER.FileAlignment)
    header_offset = pe.sections[-1].get_file_offset() + 40
    if header_offset + 40 > min(s.PointerToRawData for s in pe.sections):
        raise RuntimeError('No unused PE section-header slot')
    output = bytearray(original)
    output.extend(b'\0'*(raw-len(output)))
    output.extend(payload)
    raw_size = align(len(payload),pe.OPTIONAL_HEADER.FileAlignment)
    output.extend(b'\0'*(raw_size-len(payload)))
    section_header = struct.pack('<8sIIIIIIHHI',b'.bfvmod\0',virtual_size,rva,raw_size,raw,0,0,0,0,0xe0000060)
    output[header_offset:header_offset+40] = section_header
    struct.pack_into('<H',output,pe.FILE_HEADER.get_field_absolute_offset('NumberOfSections'),len(pe.sections)+1)
    struct.pack_into('<I',output,pe.OPTIONAL_HEADER.get_field_absolute_offset('SizeOfImage'),align(rva+virtual_size,pe.OPTIONAL_HEADER.SectionAlignment))
    struct.pack_into('<I',output,pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'),0)
    patches=[]
    extra = json.loads((PROJECT/'extra-patches.json').read_text()).get(target, [])
    added_patches = [(int(p['vtable_slot'],16),int(p['expected_function'],16),p['replacement']) for p in extra]
    for location, expected, name in [*spec['patches'], *added_patches]:
        offset = pe.get_offset_from_rva(location-pe.OPTIONAL_HEADER.ImageBase)
        actual = struct.unpack_from('<I', original, offset)[0]
        if actual != expected: raise RuntimeError(f'Vtable guard failed at {location:x}')
        struct.pack_into('<I',output,offset,symbols[name])
        patches.append(dict(vtable_slot=f'{location:08x}',original=f'{expected:08x}',replacement=f'{symbols[name]:08x}',source=name))
    entry_patches=[];entry_trampolines=[]
    constant_catalog=json.loads((PROJECT/'constant-returns.json').read_text())['targets'][target]
    if constant_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Constant catalog hash differs from the ABI target')
    constant_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in constant_catalog['entries']]
    getter_catalog=json.loads((PROJECT/'word-getters.json').read_text())['targets'][target]
    if getter_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Getter catalog hash differs from the ABI target')
    getter_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in getter_catalog['entries']]
    accessor_catalog=json.loads((PROJECT/'field-accessors.json').read_text())['targets'][target]
    if accessor_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Accessor catalog hash differs from the ABI target')
    accessor_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in accessor_catalog['entries']]
    init_catalog=json.loads((PROJECT/'static-initializers.json').read_text())['targets'][target]
    if init_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Initializer catalog hash differs from the ABI target')
    init_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in init_catalog['entries']]
    deleting_catalog=json.loads((PROJECT/'deleting-destructors.json').read_text())['targets'][target]
    if deleting_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Deleting-destructor catalog hash differs from the ABI target')
    deleting_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in deleting_catalog['entries']]
    construct_catalog=json.loads((PROJECT/'constructor-calls.json').read_text())['targets'][target]
    if construct_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Constructor-call catalog hash differs from the ABI target')
    construct_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in construct_catalog['entries']]
    cast_catalog=json.loads((PROJECT/'dynamic-casts.json').read_text())['targets'][target]
    if cast_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Dynamic-cast catalog hash differs from the ABI target')
    cast_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in cast_catalog['entries']]
    clone_catalog=json.loads((PROJECT/'clones.json').read_text())['targets'][target]
    if clone_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Clone catalog hash differs from the ABI target')
    clone_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in clone_catalog['entries']]
    store_catalog=json.loads((PROJECT/'global-stores.json').read_text())['targets'][target]
    if store_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Global-store catalog hash differs from the ABI target')
    store_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in store_catalog['entries']]
    hand_entries=[
        (spec['target_history'],bytes.fromhex('83ec105657'),'bfv_target_history'),
        (spec['target_history_insert'],bytes.fromhex('51558b6c2410'),'bfv_history_insert'),
        (spec['history_insert_node'],bytes.fromhex('83ec445657'),'bfv_history_insert_node'),
        (spec['tree_previous'],bytes.fromhex('8b018a5015'),'bfv_tree_previous'),
        (spec['tree_rotate_left'],bytes.fromhex('8b5424048b4208'),'bfv_tree_rotate_left'),
        (spec['tree_rotate_right'],bytes.fromhex('8b5424048b02'),'bfv_tree_rotate_right'),
        (spec['tree_construct'],bytes.fromhex('8b5424088bc1'),'bfv_tree_construct'),
        (spec['artillery_filter'],bytes.fromhex('8b4424408b4008'),'bfv_artillery_filter_bridge'),
        (spec['artillery_weapons'],bytes.fromhex('8b4c24508b11'),'bfv_artillery_weapons_bridge'),
        (spec['artillery_aim_gate'],bytes.fromhex('8b4424188a4814'),'bfv_artillery_aim_gate_bridge'),
        (spec['artillery_movement_gate'],bytes.fromhex('8b7c243085ff'),'bfv_artillery_movement_gate_bridge'),
        (spec['artillery_movement_scoring'],bytes.fromhex('85ff0f842a020000'),'bfv_artillery_movement_score_bridge'),
        (spec['vector_cross_assign'],bytes.fromhex('83ec0c8bc1'),'bfv_vector_cross_assign'),
        (spec['component_event2_scalar'],bytes.fromhex('8b41048b4820'),'bfv_component_event2_scalar'),
        (spec['point_xz'],bytes.fromhex('8bc28b118910'),'bfv_point_xz'),
        (spec['point_in_symmetric_bounds'],bytes.fromhex('d90183ec08'),'bfv_point_in_symmetric_bounds'),
        (spec['region_contains_point2'],bytes.fromhex('8a81c4010000'),'bfv_region_contains_point2'),
        (spec['region_contains_point3'],bytes.fromhex('83ec08568bf1'),'bfv_region_contains_point3'),
        (spec['artillery_region_score'],bytes.fromhex('8b84249c000000'),'bfv_artillery_region_score_bridge'),
        (spec['list_find_word'],bytes.fromhex('568b742408'),'bfv_find_word_in_nodes'),
        (spec['artillery_category_score'],bytes.fromhex('f64507010f8475010000'),'bfv_artillery_category_score_bridge'),
        (spec['target_handle_eligible'],bytes.fromhex('51568bc125ffff0000'),'bfv_target_handle_eligible'),
        (spec['artillery_alternate_gate'],bytes.fromhex('8b4d10c1e903'),'bfv_artillery_alternate_gate_bridge'),
        (spec['artillery_alternate_accept'],bytes.fromhex('8d472485c0'),'bfv_artillery_nested_score_bridge'),
        (spec['artillery_nested_finish'],bytes.fromhex('d944242851'),'bfv_artillery_alternate_finish_bridge'),
        (spec['artillery_next_target'],bytes.fromhex('8b49208b542404'),'bfv_next_target_handle'),
        (spec['artillery_filter_reject'],bytes.fromhex('8b5424408b3a'),'bfv_artillery_next_candidate_bridge'),
        (spec['artillery_after_first_pass'],bytes.fromhex('8b4c24508b11'),'bfv_artillery_second_setup_bridge'),
        (spec['artillery_second_filter'],bytes.fromhex('8b44241c8b00'),'bfv_artillery_second_filter_bridge'),
        (spec['artillery_second_accept'],bytes.fromhex('8b4c241c8b16'),'bfv_artillery_second_weight_bridge'),
        (spec['vector_normalize'],bytes.fromhex('51d94108d94104'),'bfv_vector_normalize'),
        (spec['artillery_second_weight_continue'],bytes.fromhex('8b8424fc010000'),'bfv_artillery_second_aim_bridge'),
        (spec['artillery_second_aim_accept'],bytes.fromhex('8b7c24186a02'),'bfv_artillery_second_position_bridge'),
        (spec['artillery_second_position_continue'],bytes.fromhex('d98424c4000000'),'bfv_artillery_second_distance_bridge'),
        (spec['artillery_second_distance_continue'],bytes.fromhex('8b4c24508b01'),'bfv_artillery_second_weapons_bridge'),
        (spec['artillery_second_weapons_accept'],bytes.fromhex('8b7c242485ff'),'bfv_artillery_second_movement_gate_bridge'),
        (spec['artillery_second_movement_continue'],bytes.fromhex('85ff0f8431020000'),'bfv_artillery_second_movement_score_bridge'),
        (spec['artillery_second_driver_gate'],bytes.fromhex('d9442420d80d')+struct.pack('<I',spec['artillery_query_scale']),'bfv_artillery_second_driver_gate_bridge'),
        (spec['artillery_second_region'],bytes.fromhex('85ff8b8c249c0000008b'),'bfv_artillery_second_region_modifier_bridge'),
        (spec['artillery_second_category_gate'],bytes.fromhex('8b4310c1e803a801'),'bfv_artillery_second_category_gate_bridge'),
        (spec['artillery_second_category_prefix'],bytes.fromhex('8b0f894c24248b4b20'),'bfv_artillery_second_category_prefix_bridge'),
        (spec['artillery_second_category_factor'],bytes.fromhex('8d432485c074058b4330eb02'),'bfv_artillery_second_category_factor_bridge'),
        (spec['artillery_second_category_score'],bytes.fromhex('8b8c24d80000008b51088b44243c'),'bfv_artillery_second_category_score_bridge'),
        (spec['artillery_second_iterator'],bytes.fromhex('8b44241c8b8c2404010000'),'bfv_artillery_second_iterator_bridge'),
        (spec['artillery_final_weights'],bytes.fromhex('8b168b3d')+struct.pack('<I',spec['artillery_parameter_manager'])+bytes.fromhex('8bceff92dc000000'),'bfv_artillery_final_weights_bridge'),
        (spec['artillery_final_selection'],bytes.fromhex('d9842480000000'),'bfv_artillery_final_selection_bridge'),
        (spec['artillery_search_prefix'],b'\x8b\x0d'+struct.pack('<I',spec['collision_clock']),'bfv_artillery_search_prefix_bridge'),
        (spec['artillery_candidate_setup'],bytes.fromhex('8b7c24188b0f'),'bfv_artillery_candidate_setup_bridge'),
        (spec['artillery_score'],bytes.fromhex('81ecd8010000'),'bfv_artillery_entry_bridge'),
        (spec['artillery_second_movement_unit_score'],bytes.fromhex('c74424340000803f'),'bfv_artillery_unit_score_bridge'),
        (spec['artillery_driver_reload'],bytes.fromhex('8bbc24fc010000'),'bfv_artillery_driver_reload_bridge'),
        (spec['artillery_allocation_reload'],bytes.fromhex('8b842400010000'),'bfv_artillery_allocation_reload_bridge'),
        (spec['artillery_second_empty'],bytes.fromhex('85c0740950'),'bfv_artillery_query_cleanup_bridge'),
        (spec['artillery_query_gate'],b'\xd9\x44\x24\x1c\xd8\x0d'+struct.pack('<I',spec['artillery_query_scale']),'bfv_artillery_query_gate_bridge'),
        (spec['component_event2_word'],bytes.fromhex('8b41048b4820'),'bfv_component_event2_word'),
        (spec['component_query_data'],bytes.fromhex('8b098b01ff5034'),'bfv_component_query_data'),
        (spec['query_vector_append'],bytes.fromhex('568b710485f6'),'bfv_query_vector_append'),
        (spec['query_vector_destroy'],bytes.fromhex('568bf18b4604'),'bfv_query_vector_destroy'),
        (spec['transform_point'],bytes.fromhex('8b442404d94108'),'bfv_transform_point'),
        (spec['vector_difference'],bytes.fromhex('8b442408d900'),'bfv_vector_difference'),
        (spec['aim_world_position'],bytes.fromhex('568bf18b4e04'),'bfv_aim_world_position'),
        (spec['component_position'],bytes.fromhex('8b41048b542404'),'bfv_component_position'),
        *[(spec[key],bytes.fromhex('d9442404d85c2408'),'bfv_'+key)
          for key in ['float_minimum','float_maximum','float_clamp']],
        (spec['vector_length'],bytes.fromhex('d94108d94104'),'bfv_vector_length'),
        (spec['aim_direction'],bytes.fromhex('8b41048b542404'),'bfv_aim_direction'),
        (spec['aim_within_limits'],bytes.fromhex('83ec405657'),'bfv_aim_within_limits'),
        (spec['aim_compose'],bytes.fromhex('8b54240856'),'bfv_affine_compose'),
        (spec['aimer_track'],bytes.fromhex('a1')+struct.pack('<I',spec['trace_index']),'bfv_aimer_track'),
        (spec['aimer_predict'],bytes.fromhex('83ec0ca1')+struct.pack('<I',spec['trace_index']),'bfv_aimer_predict'),
        (spec['aimer_projectile_offset'],bytes.fromhex('83ec1cd905')+struct.pack('<I',spec['math_zero']),'bfv_aimer_projectile_offset'),
        (spec['aimer_solve'],bytes.fromhex('558bec83e4f8'),'bfv_aimer_solve'),
        (spec['strategic_same_cell'],bytes.fromhex('538bc18b4828'),'bfv_strategic_same_cell'),
        (spec['strategic_cell_point'],bytes.fromhex('538b5c240c'),'bfv_strategic_cell_point'),
        (spec['strategic_link_z'],bytes.fromhex('8b5424048bc1'),'bfv_strategic_link_z'),
        (spec['strategic_link_x'],bytes.fromhex('8b5424048bc1'),'bfv_strategic_link_x'),
        (spec['strategic_link_back_z'],bytes.fromhex('8b44240c8b542404'),'bfv_strategic_link_back_z'),
        (spec['strategic_link_back_x'],bytes.fromhex('8b542404a1')+struct.pack('<I',spec['strategic_x_mask']),'bfv_strategic_link_back_x'),
        (spec['astar_strategic_start'],bytes.fromhex('8b44240483c134'),'bfv_astar_strategic_start'),
        (spec['astar_strategic_goal'],bytes.fromhex('8b44240483c140'),'bfv_astar_strategic_goal'),
        (spec['astar_strategic_estimate'],bytes.fromhex('8b5148568b742408'),'bfv_astar_strategic_estimate'),
        (spec['astar_strategic_reached'],bytes.fromhex('538b5c240856'),'bfv_astar_strategic_reached'),
        (spec['astar_strategic_within'],bytes.fromhex('538b19568b742410'),'bfv_astar_strategic_within'),
        (spec['astar_local_estimate'],bytes.fromhex('5153568bf18b462c'),'bfv_astar_local_estimate'),
        (spec['astar_local_reached'],bytes.fromhex('53568bf18b869c00'),'bfv_astar_local_reached'),
        (spec['astar_local_set_start'],bytes.fromhex('51568bf18b464c85'),'bfv_astar_local_set_start'),
        (spec['astar_local_set_goal'],bytes.fromhex('51568bf18b465085'),'bfv_astar_local_set_goal'),
        (spec['astar_local_add_cell'],bytes.fromhex('83ec508b54245853'),'bfv_astar_local_add_cell'),
        (spec['astar_local_add_cell_avoiding'],bytes.fromhex('83ec508b54245853'),'bfv_astar_local_add_cell_avoiding'),
        (spec['astar_local_successors'],bytes.fromhex('8b44240881ec5801'),'bfv_astar_local_successors'),
        (spec['astar_strategic_step'],bytes.fromhex('83ec14538b5c241c'),'bfv_astar_strategic_step'),
        (spec['astar_strategic_expand'],bytes.fromhex('5153558b6c2418'),'bfv_astar_strategic_expand'),
        (spec['astar_strategic_setup'],bytes.fromhex('83ec1853568bf1'),'bfv_astar_strategic_setup'),
        (spec['bitset_init'],bytes.fromhex('8b442404568bf1'),'bfv_bitset_init'),
        (spec['astar_strategic_run'],bytes.fromhex('83ec2053558be9'),'bfv_astar_strategic_run'),
        (spec['task_sleep'],bytes.fromhex('dd05')+struct.pack('<I',spec['task_minus_one']),'bfv_task_sleep'),
        (spec['task_tree_remove'],bytes.fromhex('8b442404535556'),'bfv_task_tree_remove'),
        (spec['scheduler_time_left'],bytes.fromhex('83ec08dd4110'),'bfv_scheduler_time_left'),
        (spec['scheduler_add'],bytes.fromhex('dd05')+struct.pack('<I',spec['task_minus_one'])+bytes.fromhex('53'),'bfv_scheduler_add'),
        (spec['scheduler_remove'],bytes.fromhex('8b4424048b116a00'),'bfv_scheduler_remove'),
        (spec['task_local_search_execute'],bytes.fromhex('83ec0c568bf1'),'bfv_task_local_search_execute'),
        (spec['task_strategic_search_execute'],bytes.fromhex('83ec0c568bf1'),'bfv_task_strategic_search_execute'),
        (spec['task_unschedule'],bytes.fromhex('8bc18b4814'),'bfv_task_unschedule'),
        (spec['task_queue'],bytes.fromhex('8b442410dd442408'),'bfv_task_queue'),
        (spec['task_player_map_execute'],bytes.fromhex('535556578bf1'),'bfv_task_player_map_execute'),
        (spec['task_strategic_player_map_execute'],bytes.fromhex('53568bf157'),'bfv_task_strategic_player_map_execute'),
        (spec['task_strategic_search_run'],bytes.fromhex('558bec83e4f8'),'bfv_task_strategic_search_run'),
        (spec['task_cell_query_execute'],bytes.fromhex('83ec2c535556'),'bfv_task_cell_query_execute'),
        (spec['strategic_cell_distance'],bytes.fromhex('538b5c240856'),'bfv_strategic_cell_distance'),
        (spec['searcher_step'],bytes.fromhex('83ec1c538b5c242c'),'bfv_searcher_step'),
        (spec['searcher_sift_down'],bytes.fromhex('5153568bf18b4604'),'bfv_searcher_sift_down'),
        (spec['searcher_heap_pop'],bytes.fromhex('568bf18b460483c0'),'bfv_searcher_heap_pop'),
        (spec['searcher_unclose'],bytes.fromhex('568b7424088b4608'),'bfv_searcher_unclose'),
        (spec['searcher_heap_update'],bytes.fromhex('558b6c2408568b75'),'bfv_searcher_heap_update'),
        (spec['searcher_heap_push'],bytes.fromhex('51535556578b7c24'),'bfv_searcher_heap_push'),
        (spec['searcher_reset'],bytes.fromhex('53568bf18b4e14'),'bfv_searcher_reset'),
        (spec['searcher_alloc'],bytes.fromhex('515355578bf98b4f'),'bfv_searcher_alloc'),
        (spec['searcher_path'],bytes.fromhex('8b49608bd133c0'),'bfv_searcher_path'),
        (spec['path_ring_extend'],bytes.fromhex('8b51048b41082bc2'),'bfv_path_ring_extend'),
        (spec['searcher_path_to_ring'],bytes.fromhex('8b51608bc233c9'),'bfv_searcher_path_to_ring'),
        (spec['searcher_closed_points'],bytes.fromhex('518b512853555633'),'bfv_searcher_closed_points'),
        (spec['searcher_run'],bytes.fromhex('83ec08dd05'),'bfv_searcher_run'),
        (spec['searcher_deliver'],bytes.fromhex('83ec2c568b7160'),'bfv_searcher_deliver'),
        (spec['cell_query_init'],bytes.fromhex('8b542408c741'),'bfv_cell_query_init'),
        (spec['player_map_search_init'],bytes.fromhex('8b4424048941'),'bfv_player_map_search_init'),
        (spec['vector_divide'],b'\xd9\x05'+struct.pack('<I',spec['math_one']),'bfv_vector_divide'),
        *constant_entries,*getter_entries,*accessor_entries,*init_entries,*deleting_entries,*construct_entries,*cast_entries,*clone_entries,*store_entries]
    lifted_patch_entries,lifted_stats,trampolines=lifted_entries(target,spec,pe,original,hand_entries) if lifted else ([],{},{})
    if mod_stubs:
        detoured={a for a,_,_ in lifted_patch_entries}
        missing=[f'{a:08x}' for a in mod_stubs if a not in detoured]
        if missing:raise RuntimeError(f'{target}: mods hook functions that do not run as C (not verified or not detourable): {", ".join(missing)}')
        lifted_patch_entries=[(a,p,mod_stubs.get(a,n)) for a,p,n in lifted_patch_entries if not MODS_ONLY or a in mod_stubs]
        lifted_stats=dict(lifted_stats,hooked=len(mod_stubs))
        print(f'  {target}: mods hook {len(mod_stubs)} function(s): {", ".join(f"{a:08x}" for a in sorted(mod_stubs))}')
    for location,expected,name in [*hand_entries,*lifted_patch_entries]:
        offset=pe.get_offset_from_rva(location-pe.OPTIONAL_HEADER.ImageBase)
        if original[offset:offset+len(expected)]!=expected:raise RuntimeError(f'Entry guard failed at {location:x}')
        replacement=symbols[name]
        if location in trampolines:
            via=trampolines[location];at=pe.get_offset_from_rva(via-pe.OPTIONAL_HEADER.ImageBase)
            if original[at:at+5]!=b'\xcc'*5:raise RuntimeError(f'Trampoline guard failed at {via:x}')
            jump=b'\xeb'+struct.pack('<b',via-location-2)
            far=b'\xe9'+struct.pack('<I',(replacement-via-5)&0xffffffff)
            output[at:at+5]=far
            entry_trampolines.append(dict(entry=f'{via:08x}',original='cc'*5,patched=far.hex(),
                                          replacement=f'{replacement:08x}',source=name,short_from=f'{location:08x}'))
        else:jump=b'\xe9'+struct.pack('<I',(replacement-location-5)&0xffffffff)+b'\x90'*(len(expected)-5)
        output[offset:offset+len(expected)]=jump
        entry_patches.append(dict(entry=f'{location:08x}',original=expected.hex(),patched=jump.hex(),
                                  replacement=f'{replacement:08x}',source=name,**({'via':f'{trampolines[location]:08x}'} if location in trampolines else {})))
    new_pe = pefile.PE(data=bytes(output))
    struct.pack_into('<I',output,pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'),new_pe.generate_checksum())
    destination=GAME/spec['output'] if output_path is None else Path(output_path).resolve()
    try:
        destination.write_bytes(output)
    except PermissionError as error:
        raise RuntimeError(f'Close {destination.name} before rebuilding: Windows locks running executable files') from error
    manifest=dict(target=target,input_sha256=spec['sha'],output=str(destination),
        output_sha256=hashlib.sha256(output).hexdigest(),payload_address=f'{address:08x}',
        payload_bytes=len(payload),symbols={k:f'{v:08x}' for k,v in symbols.items()},patches=patches,entry_patches=entry_patches,entry_trampolines=entry_trampolines,lifted=lifted_stats,
        scope='Reconstructed AI interpreter, bailout logic/math, collision callback/dispatcher and distance geometry, event construction, object/interface lookups and word-vector insertion/exception bridge; artillery driver/cache validation, target-history lookup/insertion and red-black balancing, and first-pass candidate filter/weapon selection; shared scalar selectors and vector length/division, direction aiming-limit predicate/event wrapper and affine matrix composition; audited constant-return functions, object-word getters, field/global accessors, C++ static-object initializers (with and without constructor arguments) scalar deleting destructors, dynamic_cast wrappers, virtual clones and constant global stores; remaining engine code/services are retained from the original image')
    (work/'manifest.json').write_text(json.dumps(manifest,indent=2))
    print(f'Compiled {destination.name}: {len(payload)} native payload bytes, {len(patches)} guarded vtable replacements and {len(entry_patches)} guarded function entries'
          +(f' ({len(lifted_patch_entries)} of them {"L1 lifted" if TREE=="lifted" else "L2 readable"} C; {lifted_stats})' if lifted_stats else ''))


def main():
    global GAME, COMPILERS, TREE, MODS_ONLY
    parser=argparse.ArgumentParser()
    parser.add_argument('--target',choices=('client','server','both'),default='both')
    parser.add_argument('--game-dir',type=Path,default=GAME)
    parser.add_argument('--compiler-bin',type=Path,default=COMPILERS)
    parser.add_argument('--no-lifted',action='store_true',help='leave out the lifted C')
    parser.add_argument('--output',type=Path,help='write the executable here instead of the game folder (one target)')
    parser.add_argument('--tree',choices=('lifted','readable'),default='lifted',
                        help='lifted C to detour into: lifted/ (L1) or readable/ (L2)')
    parser.add_argument('--mods-only',action='store_true',
                        help='run only the functions mods hook as C; the rest of the game stays original code (needs --tree readable)')
    parser.add_argument('--mods',help='build only these mods (file names without .c, comma-separated)')
    args=parser.parse_args()
    if args.mods_only and args.tree!='readable':parser.error('--mods-only needs --tree readable')
    MODS_ONLY=args.mods_only
    if args.mods is not None:
        import mods
        mods.ONLY=set(args.mods.split(','))
        missing=[n for n in mods.ONLY if not any((mods.SOURCES/d/f'{n}.c').is_file() for d in ('client','server','shared'))]
        if missing:parser.error(f'--mods: no mod file {", ".join(missing)}')
    if args.output and args.target=='both':parser.error('--output needs --target client or server')
    TREE=args.tree
    GAME=args.game_dir.resolve()
    COMPILERS=args.compiler_bin.resolve()
    for target in ('client','server') if args.target=='both' else (args.target,): build(target,not args.no_lifted,args.output)


if __name__=='__main__': main()
